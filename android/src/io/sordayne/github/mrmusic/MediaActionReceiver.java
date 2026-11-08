package io.sordayne.github.mrmusic;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * Turns media notification button presses into queued actions for
 * {@link MediaSessionHelper#pollAction()}.
 */
public class MediaActionReceiver extends BroadcastReceiver {

    public static final String ACTION_MEDIA_BUTTON = "io.sordayne.github.mrmusic.MEDIA_BUTTON";
    public static final String EXTRA_ACTION = "action";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null || !ACTION_MEDIA_BUTTON.equals(intent.getAction())) {
            return;
        }
        int action = intent.getIntExtra(EXTRA_ACTION, MediaSessionHelper.ACTION_NONE);
        switch (action) {
            case MediaSessionHelper.ACTION_PLAY_PAUSE:
            case MediaSessionHelper.ACTION_NEXT:
            case MediaSessionHelper.ACTION_PREVIOUS:
            case MediaSessionHelper.ACTION_STOP:
                MediaSessionHelper.queueAction(action);
                break;
            default:
                break;
        }
    }
}
