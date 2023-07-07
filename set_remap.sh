#!/bin/bash
host="$1"
[[ -n $host ]] || { echo "Host not given" >&2 ; exit 1 ; }
curl --fail-with-body  http://$host/remap'?id=28-ff3c01e07679b5&to=boiler.flow'
curl --fail-with-body  http://$host/remap'?id=28-ff3c01e076b715&to=boiler.rethw'
curl --fail-with-body  http://$host/remap'?id=28-ff3c01e076ccfc&to=boiler.retmain'
curl --fail-with-body  http://$host/remap'?id=28-ff3c01e076008c&to=boiler.retextn'
curl --fail-with-body  http://$host/remap'?id=28-ff641e8343e328&to=tank.hw'
curl --fail-with-body  http://$host/remap'?id=28-ff641e833c7e6a&to=tank.flow'
curl --fail-with-body  http://$host/remap'?id=28-ff641e83261b3b&to=tank.cold'
curl --fail-with-body  http://$host/remap'?id=28-ff641e83397af4&to=tank.ret'
curl --fail-with-body  http://$host/remap'?id=28-ff641e8350713c&to=drawingroom'
