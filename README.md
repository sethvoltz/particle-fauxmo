Fauxmo for Particle
===================

This is a port of the fine work done by [makermusings][] on the [fauxmo][] over to C++ and the Particle ecosystem. Fair warning, I am not a systems developer by day. My stock and trade consists of Javascript, Ruby, and (when pressed) some Java.

Pull requests are welcome and appreciated.

[makermusings]: https://github.com/makermusings
[fauxmo]: https://github.com/makermusings/fauxmo

Setting Up
----------

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


Many Thanks
-----------

These blog posts and other resources were mighty helpful in getting this working. Many thanks for the time and effort it took for these fine folks to share their work with the world!

- http://www.makermusings.com/2015/07/13/amazon-echo-and-home-automation
- https://objectpartners.com/2014/03/25/a-groovy-time-with-upnp-and-wemo
- https://github.com/smpickett/particle_ssdp_server


TODOs and Wishlist
------------------

Pull requests very welcome for any bugs, issues, and features, expecially these upcoming ones.

- [ ] Periodically sync time as well as any other Particle housekeeping tasks required for always-on devices
- [x] Enable a physical switch for controlling the device
- [ ] Enable a second physical switch for controlling another network device (call REST endpoint or send UPnP to another FauxMo)
- [ ] Convert main timing code from `millis()` tracking to FreeRTOS software timers
- [ ] Extract all UPnP stuff unto a library
- [ ] Extend the UPnP library to allow multiple virtual devices, each with their own digital output and state control
- [ ] Extend UPnP library to respond to a native device search, and differentiate between WeMo, our own, and `**` requests.
