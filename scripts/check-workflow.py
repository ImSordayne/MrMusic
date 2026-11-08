import sys
import yaml

path = sys.argv[1] if len(sys.argv) > 1 else ".github/workflows/build.yml"
with open(path, encoding="utf-8") as handle:
    data = yaml.safe_load(handle)

print("name:", data.get("name"))
print("triggers:", list(data.get(True, data.get("on", {})).keys()))
print("env:", data.get("env"))
print()
for job_id, job in data["jobs"].items():
    print(f"job {job_id:8s} runs-on={job.get('runs-on'):16s} "
          f"needs={job.get('needs', '-')!s:24s} if={job.get('if', '-')}")
    for step in job.get("steps", []):
        label = step.get("name") or step.get("uses") or "run"
        print(f"    - {label}")
print()
print("YAML OK")
