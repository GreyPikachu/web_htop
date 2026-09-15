# Container and Kubernetes deployment

## Build

Build the native image and load it into Docker:

```bash
docker buildx build --load --tag web-htop:local .
```

Build and publish a multi-architecture image:

```bash
docker buildx create --use --name web-htop-builder
docker run --privileged --rm tonistiigi/binfmt --install arm64,amd64
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  --tag ghcr.io/romansnitko/web_htop:latest \
  --push .
```

The builder stage executes on the target platform. BuildKit uses QEMU for the
non-native architecture; this avoids accidentally copying an amd64 executable
into an arm64 image.

## Local smoke test

```bash
bash scripts/container_smoke.sh
```

The test starts the server as UID/GID 10001, drops every Linux capability,
uses a read-only root filesystem, mounts host telemetry sources read-only and
checks `/health` and `/ready`.

## Kubernetes

Set the image tag in `packaging/k8s/kustomization.yaml`, then deploy:

```bash
kubectl apply -k packaging/k8s
kubectl rollout status daemonset/web-htop
kubectl get pods -l app.kubernetes.io/name=web-htop -o wide
```

Forward the HTTP API of one selected pod:

```bash
pod="$(kubectl get pods -l app.kubernetes.io/name=web-htop \
  -o jsonpath='{.items[0].metadata.name}')"
kubectl port-forward "pod/$pod" 8080:8080
curl http://127.0.0.1:8080/ready
```

## Security model

The default manifest does not use `privileged: true`. It runs as a numeric
non-root user, prevents privilege escalation, drops all capabilities, uses the
runtime-default seccomp profile and mounts host paths read-only.

Host-wide telemetry still crosses an isolation boundary. The DaemonSet uses
`hostPID: true` and read-only `hostPath` volumes, so it does **not** satisfy the
Kubernetes Pod Security Standards `baseline` or `restricted` profiles.

Most `/proc/<pid>/stat`, `status` and `comm` data is normally readable without
extra capabilities. Nodes configured with `hidepid` or stricter LSM policies
may expose only partial process data. If the cluster owner accepts the wider
read boundary, deploy the explicit overlay:

```bash
kubectl apply -k packaging/k8s/overlays/proc-access
```

That overlay adds only `DAC_READ_SEARCH` and `SYS_PTRACE`. Do not enable it by
default, and do not describe it as Pod Security `restricted` compliant.
