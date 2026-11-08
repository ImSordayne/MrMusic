package io.sordayne.github.mrmusic;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.media.MediaMetadata;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Build;

import java.util.ArrayDeque;

/**
 * Native "now playing" support for Android: a {@link MediaSession} plus a
 * media style notification with play/pause/next/previous actions.
 *
 * The Java side never calls back into C++ directly; transport button presses are
 * queued and drained by the C++ implementation of
 * {@code mr::MediaControls} (see src/player/MediaControls.cpp) through
 * {@link #pollAction()}.
 */
public class MediaSessionHelper {

    public static final int ACTION_NONE = -1;
    public static final int ACTION_PLAY_PAUSE = 0;
    public static final int ACTION_NEXT = 1;
    public static final int ACTION_PREVIOUS = 2;
    public static final int ACTION_STOP = 3;

    private static final String CHANNEL_ID = "mrmusic.playback";
    private static final int NOTIFICATION_ID = 0x4d52;
    private static final ArrayDeque<Integer> PENDING = new ArrayDeque<>();

    private final Context context;
    private final NotificationManager notificationManager;
    private final MediaSession session;

    private String title = "";
    private String artist = "";
    private String album = "";
    private boolean playing = false;
    private boolean prepared = false;

    public MediaSessionHelper(Context context) {
        this.context = context;
        this.notificationManager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        this.session = new MediaSession(context, "MrMusic");
        this.session.setCallback(new MediaSession.Callback() {
            @Override
            public void onPlay() {
                push(ACTION_PLAY_PAUSE);
            }

            @Override
            public void onPause() {
                push(ACTION_PLAY_PAUSE);
            }

            @Override
            public void onSkipToNext() {
                push(ACTION_NEXT);
            }

            @Override
            public void onSkipToPrevious() {
                push(ACTION_PREVIOUS);
            }

            @Override
            public void onStop() {
                push(ACTION_STOP);
            }
        });
        this.session.setActive(true);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID, "MrMusic", NotificationManager.IMPORTANCE_LOW);
            channel.setShowBadge(false);
            notificationManager.createNotificationChannel(channel);
        }
    }

    /** Enqueues a transport action (used by {@link MediaActionReceiver}). */
    public static void queueAction(int action) {
        synchronized (PENDING) {
            PENDING.add(action);
        }
    }

    private static void push(int action) {
        queueAction(action);
    }

    /** Called from C++ every ~250 ms. */
    public static int pollAction() {
        synchronized (PENDING) {
            Integer value = PENDING.poll();
            return value == null ? ACTION_NONE : value;
        }
    }

    public void updateMetadata(String title, String artist, String album) {
        this.title = title == null ? "" : title;
        this.artist = artist == null ? "" : artist;
        this.album = album == null ? "" : album;

        MediaMetadata metadata = new MediaMetadata.Builder()
                .putString(MediaMetadata.METADATA_KEY_TITLE, this.title)
                .putString(MediaMetadata.METADATA_KEY_ARTIST, this.artist)
                .putString(MediaMetadata.METADATA_KEY_ALBUM, this.album)
                .build();
        session.setMetadata(metadata);
        updatePlaybackState();
        postNotification();
    }

    public void setPlaybackState(boolean playing) {
        this.playing = playing;
        updatePlaybackState();
        postNotification();
    }

    private void updatePlaybackState() {
        long actions = PlaybackState.ACTION_PLAY
                | PlaybackState.ACTION_PAUSE
                | PlaybackState.ACTION_PLAY_PAUSE
                | PlaybackState.ACTION_SKIP_TO_NEXT
                | PlaybackState.ACTION_SKIP_TO_PREVIOUS
                | PlaybackState.ACTION_STOP;
        PlaybackState.Builder builder = new PlaybackState.Builder()
                .setActions(actions)
                .setState(playing ? PlaybackState.STATE_PLAYING : PlaybackState.STATE_PAUSED,
                        PlaybackState.PLAYBACK_POSITION_UNKNOWN, playing ? 1.0f : 0.0f);
        session.setPlaybackState(builder.build());
    }

    private PendingIntent contentIntent() {
        Intent intent = new Intent(context, org.qtproject.qt.android.bindings.QtActivity.class);
        intent.setAction(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            flags |= PendingIntent.FLAG_IMMUTABLE;
        }
        return PendingIntent.getActivity(context, 0, intent, flags);
    }

    private PendingIntent actionIntent(int action, String label) {
        Intent intent = new Intent(context, MediaActionReceiver.class);
        intent.setAction(MediaActionReceiver.ACTION_MEDIA_BUTTON);
        intent.putExtra(MediaActionReceiver.EXTRA_ACTION, action);
        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            flags |= PendingIntent.FLAG_IMMUTABLE;
        }
        return PendingIntent.getBroadcast(context, action, intent, flags);
    }

    private void postNotification() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return;
        }
        Notification.Builder builder = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                ? new Notification.Builder(context, CHANNEL_ID)
                : new Notification.Builder(context);

        Notification.MediaStyle style = new Notification.MediaStyle()
                .setMediaSession(session.getSessionToken())
                .setShowActionsInCompactView(0, 1, 2);

        builder.setStyle(style)
                .setContentTitle(title)
                .setContentText(artist)
                .setSmallIcon(android.R.drawable.ic_media_play)
                .setContentIntent(contentIntent())
                .setOngoing(playing);

        builder.addAction(new Notification.Action.Builder(
                playing ? android.R.drawable.ic_media_pause : android.R.drawable.ic_media_play,
                "Play/Pause", actionIntent(ACTION_PLAY_PAUSE, "toggle")).build());
        builder.addAction(new Notification.Action.Builder(
                android.R.drawable.ic_media_previous,
                "Previous", actionIntent(ACTION_PREVIOUS, "prev")).build());
        builder.addAction(new Notification.Action.Builder(
                android.R.drawable.ic_media_next,
                "Next", actionIntent(ACTION_NEXT, "next")).build());

        try {
            notificationManager.notify(NOTIFICATION_ID, builder.build());
        } catch (SecurityException ignored) {
            // notifications permission revoked - playback continues
        }
        prepared = true;
    }

    public boolean isPrepared() {
        return prepared;
    }

    public void release() {
        session.setActive(false);
        session.release();
        notificationManager.cancel(NOTIFICATION_ID);
    }
}
