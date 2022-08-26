# Polycom-MPTZ-Protocol
Reverse engineering for the Polycom MPTZ series of cameras

## Initial investigation

Serial connection at 9600 8-E-1

Device issues `e0` until initialized with `81 40`. Initialization code can be sent at any time.

```python
import serial
s = serial.Serial('/dev/ttyUSB0',9600,parity=serial.PARITY_EVEN)
s.write(b'\x81\x40')            # init (expect a0 93 40 01 10)
s.write(b'\x82\x06\x77')        # get fw info
# these commands just expect an ok response (a0 92 40 00)
s.write(b'\x84\x43\x02\x04\x55')# set zoom 119.5 deg fov
s.write(b'\x8a\x41\x50\x3a\x03\x51\x00\x5a\x7f\x7f\x00') # set position P=97.7 T=21.8
```

## PTZ

```text
84 43 02 09 2b                      # zoom to 119.5 deg fov
8a 41 50 3a 02 0c 00 5a 7f 7f 00    # set position 400 200 64
                                    # API 652 218 1195
                                    # P 65.2 degrees T 21.8 degrees
                                    # Hex to Dec PTZ
                                    # 524 90 127/32639

84 43 02 09 2b                      # zoom to 119.5 deg fov
8a 41 50 3a 03 51 00 5a 7f 7f 00    # set position 600 200 64
                                    # API 977 218 1195
                                    # P 97.7 degrees T 21.8 degrees
                                    # Hex to Dec PTZ
                                    # 849 90 127/32639

8a 41 50 32 03 51 00 6d 7f 7f 00    # set position 600 100 64
                                    # API 977 109 1195
                                    # P 97.7 T 10.9
                                    # 6d = 109

84 43 02 04 55                      # zoom (84 43 02)
                                    # 092b/2347 = 64 / 119.5 deg fov
                                    # 0455/1109 = 32 /  59.7 deg fov
                                    # 022a/554  = 16 /  29.8 deg fov
8a 41 50 32 03 51 00 6d 7f 7f 00    # set position 600 100 32

Breakdown
    Send wake up                    84 43 02 09 2b
    Expect ok                       a0 92 40 00

    Move to                         8a 41 50 3a
    16 bit pan                      degrees * 10 - 128 (97.7 = 977-128 = 849 = 03 51)
    16 bit tilt                     degrees * 10 (10.9 * 10 = 109)
    ???                             7f 7f 00
```

Building an individual move command
```text
84    # start
43    # move
XX    # move how? 02 = zoom, 04 = pan, 05 = tilt
XX    # MSB (big stepping)
      # Pan  00 = leftmost - 0f = rightmost (from camera's perspective)
      # Tilt 01 = down 02 = up
      # Zoom 01 = near - 11 = far
XX    # LSB (little stepping)
      # Pan  00 - cf
      # Tilt 00 - ff
      # Zoom 00 - ff
```
This might be good for an Arduino hooked up with tactile switches for a super simple control scheme. It would be easy to control using only the MSB since the stepping is in the goldilocks zone. Sending a command outside the range is simply ignored.

Because pan centering is best at `c0` if using a simple Arduino control setting the LSB to `c0` for pan is a good idea. Fine control might be achieved with a rotary encoder?

Examples
```text
84 43 02 00 00 # Zoom to nearest
84 43 04 06 c0 # Pan to center
84 43 05 02 00 # Tilt to center
```
