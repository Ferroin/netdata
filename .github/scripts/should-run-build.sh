#!/bin/sh

event="${1}"

DOCKER_COMPONENT_REGEX="packaging/docker/.*|\.github/workflows/docker\.yml"
INSTALLER_COMPONENT_REGEX="netdata-installer\.sh|packaging/installer/functions.sh|packaging/[^/]*.(version|checksums)"
BUILD_COMPONENT_REGEX="configure\.ac|Makefile|build/\.*|.*\.[ch]|.*\.cc"
SUBMODULE_REGEX="aclk/aclk-schemas|mqtt_websockets|ml/kmeans/dlib|ml/json"

run_docker="false"

file_list="$(git diff --name-only master HEAD)"

should_run_docker() {
    if echo "${file_list}" | grep -Eq "${DOCKER_COMPONENT_REGEX}"; then
        run_docker="true"
    elif echo "${file_list}" | grep -Eq "${INSTALLER_COMPONENT_REGEX}"; then
        run_docker="true"
    elif echo "${file_list}" | grep -Eq "${BUILD_COMPONENT_REGEX}"; then
        run_docker="true"
    elif echo "${file_list}" | grep -Eq "${SUBMODULE_REGEX}"; then
        run_docker="true"
    fi
}

case "${event}" in
    pull_request|push)
        should_run_docker
        ;;
    *)
        run_docker="true"
esac

echo "::set-output name=run-docker::${run_docker}"
