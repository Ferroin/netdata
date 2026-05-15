#!/bin/bash

set -euo pipefail

url="$(cat packaging/vendor/ibm_mq.url)"
sha256="$(cat packaging/vendor/ibm_mq.sha256)"
file="${url##*/}"
cache_dir="artifacts/cache"
tmp_dir="tmp"
hash_path="${tmp_dir}/hash"
dl_path="${tmp_dir}/${file}"
dl_log="${tmp_dir}/dl.log"
cache_path="${cache_dir}/${file}"
extract_path="${tmp_dir}/ibm_mq"
retry_delay=90
need_to_fetch=1

rm -rf "${extract_path}"
mkdir -p "${tmp_dir}" "${cache_dir}" "${extract_path}"

fetch_url() {
    success=0

    for i in $(seq 5) ; do
        echo "Download attempt ${i}"
        set +e
        rm -f "${dl_log}"
        curl --output "${dl_path}" \
             --fail \
             --location \
             --max-time 300 \
             --connect-timeout 90 \
             --remote-time \
             --write-out "%{http_code}" \
             "${url}" > "${dl_log}"
        ret="$?"
        set -e

        case "${ret}" in
            0)
                echo "Download successful."
                break
                ;;
            22|78)
                status="$(tail -n 1 "${dl_log}")"
                case "${status}" in
                    403|404)
                        echo "Download failed, got ${status} from remote server."
                        exit 1
                        ;;
                    *) echo "Download failed, got ${status} from remote server, retrying in ${retry_delay} seconds." ;;
                esac
                ;;
            5|6|7) echo "Failed to connect to remote server, retrying in ${retry_delay} seconds." ;;
            35|60|83) echo "TLS error connecting to remote server, retrying in ${retry_delay} seconds." ;;
            *)
                echo "Unknown error (exit code ${ret}) downloading remote file."
                exit 1
                ;;
        esac

        sleep "${retry_delay}"
    done
}

hash_valid() {
    local path="${1}"
    echo "${sha256}  ${path}" > "${hash_path}"
    if sha256sum -c "${hash_path}"; then
        return 0
    else
        return 1
    fi
}

if [ -f "${cache_path}" ]; then
    if ! hash_valid "${cache_path}" ; then
        rm -f "${cache_path}"
    else
        need_to_fetch=0
    fi
fi

if [ "${need_to_fetch}" -eq 1 ]; then
    rm -f "${dl_path}"
    fetch_url
    hash_valid "${dl_path}"
    cp "${dl_path}" "${cache_path}"
fi

tar -xvzf "${cache_path}" -C "${extract_path}"
