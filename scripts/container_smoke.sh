#!/usr/bin/env bash
set -Eeuo pipefail

readonly image="${WEB_HTOP_IMAGE:-web-htop:smoke}"
readonly container="web-htop-smoke-${RANDOM}-$$"
readonly http_port="${WEB_HTOP_SMOKE_HTTP_PORT:-18080}"

for command in docker curl; do
    command -v "$command" >/dev/null 2>&1 || {
        echo "error: '$command' is required" >&2
        exit 1
    }
done

cleanup() {
    docker rm --force "$container" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

case "$(uname -m)" in
    x86_64) platform="linux/amd64" ;;
    aarch64|arm64) platform="linux/arm64" ;;
    *)
        echo "error: unsupported smoke-test architecture: $(uname -m)" >&2
        exit 1
        ;;
esac

if [[ "${WEB_HTOP_SKIP_BUILD:-0}" != "1" ]]; then
    docker buildx build --load --platform "$platform" --tag "$image" .
fi

docker run --detach \
    --name "$container" \
    --read-only \
    --cap-drop ALL \
    --user 10001:10001 \
    --publish "127.0.0.1:${http_port}:8080" \
    --volume /proc:/host/proc:ro \
    --volume /sys:/host/sys:ro \
    --volume /:/host/root:ro \
    "$image" \
    --proc-root /host/proc \
    --sys-root /host/sys \
    --mount /host/root \
    --bind 0.0.0.0

for attempt in $(seq 1 30); do
    if curl --fail --silent --show-error "http://127.0.0.1:${http_port}/health" >/dev/null; then
        break
    fi
    if (( attempt == 30 )); then
        docker logs "$container" >&2
        echo "error: /health did not become available" >&2
        exit 1
    fi
    sleep 1
done

for attempt in $(seq 1 30); do
    if curl --fail --silent         "http://127.0.0.1:${http_port}/ready" >/dev/null; then
        break
    fi

    if (( attempt == 30 )); then
        docker logs "$container" >&2
        echo "error: /ready did not become available" >&2
        exit 1
    fi

    sleep 1
done

curl --fail --silent --show-error     "http://127.0.0.1:${http_port}/health"
echo

curl --fail --silent --show-error     "http://127.0.0.1:${http_port}/ready"
echo

actual_user="$(docker inspect --format '{{.Config.User}}' "$container")"
[[ "$actual_user" == "10001:10001" ]] || {
    echo "error: unexpected image user: $actual_user" >&2
    exit 1
}

readonly_rootfs="$(docker inspect --format '{{.HostConfig.ReadonlyRootfs}}' "$container")"
[[ "$readonly_rootfs" == "true" ]] || {
    echo "error: root filesystem is writable" >&2
    exit 1
}

echo "container smoke test passed ($platform, user $actual_user, read-only rootfs)"
