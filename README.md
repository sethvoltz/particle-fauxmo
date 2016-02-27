Fauxmo for Particle
-------------------

This is a port of the fine work done by [makermusings][] on the [fauxmo][] over to C++ and the Particle ecosystem. Fair warning, I am not a systems developer by day. My stock and trade consists of Javascript, Ruby, and (when pressed) some Java.

Pull requests are welcome and appreciated.

[makermusings]: https://github.com/makermusings
[fauxmo]: https://github.com/makermusings/fauxmo

Setting Up
==========

I prefer to use multiple terminals so I can watch the serial output of the device while throwing network requests at it. Here are the commands I use:

* Terminal 1: Build, flash and monitor
  ```
  $ particle compile photon ./ --saveTo firmware.bin && \
  particle flash <device_name> firmware.bin && \
  sleep 7 && particle serial monitor
  ```

* Terminal 2: Monitor UPnP multicast traffic
  ```
  $ socat UDP4-RECVFROM:1900,ip-add-membership=239.255.255.250:0.0.0.0,fork -
  ```

* Terminal 3: Send UPnP & HTTP Requests
  ```
  $ pbpaste | socat STDIO UDP4-DATAGRAM:239.255.255.250:1900,broadcast
  ```
  ```
  $ http "http://10.0.0.31:49153/setup.xml"
  ```
  ```
  $ http post http://10.0.0.31:49153/upnp/control/basicevent1 \
  SOAPACTION:'"urn:Belkin:service:basicevent:1#SetBinaryState"' \
  body="<BinaryState>1</BinaryState>"
  ```
