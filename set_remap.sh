#!/bin/bash
host="$1"
[[ -n $host ]] || { echo "Host not given" >&2 ; exit 1 ; }
curl --fail-with-body  http://$host/remap'?id=28-b57976e0013cd3&to=boiler.flow'
curl --fail-with-body  http://$host/remap'?id=28-15b776e0013c0f&to=boiler.rethw'
curl --fail-with-body  http://$host/remap'?id=28-fccc76e0013cad&to=boiler.retmain'
curl --fail-with-body  http://$host/remap'?id=28-8c0076e0013c36&to=boiler.retextn'
curl --fail-with-body  http://$host/remap'?id=28-ff641e8343e328&to=tank.hw'
curl --fail-with-body  http://$host/remap'?id=28-ff641e833c7e6a&to=tank.flow'
curl --fail-with-body  http://$host/remap'?id=28-ff641e83261b3b&to=tank.cold'
curl --fail-with-body  http://$host/remap'?id=28-ff641e83397af4&to=tank.ret'
curl --fail-with-body  http://$host/remap'?id=28-ff641e8350713c&to=drawingroom'
