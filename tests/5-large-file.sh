#!/bin/bash

PROXY_HOST="localhost"
PROXY_PORT="8080"
URL="http://xcal1.vodafone.co.uk/1GB.zip"
REQUESTS=5
CURL_OPTIONS="--http1.0 --proxy1.0 ${PROXY_HOST}:${PROXY_PORT}"

make-request() {
    NUM=$1

    time=$(curl -v -s -w "%{time_total}\n" -o results/result-${NUM}.zip ${CURL_OPTIONS} ${URL})
    echo "${NUM}: ${time}s"
}

rm -r results
mkdir -p results

for i in `seq ${REQUESTS}`; do
    make-request ${i}
done

wait

echo -ne "\nComparing results: "
for i in `seq ${REQUESTS}`; do
    if ! diff results/result-1.zip results/result-${i}.zip -q; then
        exit 1
    fi
done

echo OK
