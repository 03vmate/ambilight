# Ambilight

The program controls a lighting system consisting of WS2812B LEDs, positioned behind a TV and along the edges of the display.
Two modes are supported:
 - Network mode - Color of LEDs are set through TCP
 - V4L2 mode - LED colors based on the image captured by a V4L2 device, like an HDMI capture card

In V4L2 mode, the average color of pixels at the edges of the image is calculated. For example, with 3840 horizontal pixels, 40 horizontal LEDs, and a "border thickness" config parameter of 30 pixels, one LED corresponds to 96x30 pixel regions along the upper and lower edges, and its color is the averaged color of the 96x30 pixel region.

In network mode, a message sent over TCP contains the desired color/brightness for all LEDs. There is a sample program in the `client_app`` folder that captures the screen using Xorg Xlib and adjusts the LED colors based on the edges of the image, similar to V4L2 capture mode.

Communication with the LED strip occurs through a microcontroller, which communicates with the LEDs based on commands received via UART/serial port. PlatformIO is required for MCU firmware build.

# Config
| Paraméter        | Mode         | Leírás                               |
|------------------|--------------|--------------------------------------|
| `mode`           | v4l2/network | `network` or `v4l2` (HDMI capture)   |
| `serial_port`    | v4l2/network | Path to serial port of MCU           |
| `baud`           | v4l2/network | MCU baud rate                        |
| `capture_device` | v4l2         | V4L2 device path                     |
| `border_size`    | v4l2/client  | Number of pixels considered at the edges of the image |
| `vertical_leds`  | v4l2/client  | Number of LEDs in the vertical direction   |
| `horizontal_leds`| v4l2/client  | Number of LEDs in the horizontal direction   |
| `capture_width`  | v4l2         | Image capture width                  |
| `capture_height` | v4l2         | Image capture height                 |
| `capture_fps`    | v4l2         | Capture FPS                          |
| `gamma_correction` | v4l2/client | Gamma value                         |
| `v4l2_buffer_count` | v4l2      | V4L2 buffer count                    |
| `sleep_after`    | v4l2         | Reduce capture FPS to 1 after this many black frames |
| `averaging_samples` | v4l2      | Average this many color samples for smoother lighting |
| `port`           | network      | Listen port for network mode         |
| `server_port`    | client       | Port of server to connect to         |
| `server_ip`      | client       | IP of server to connect to           |

\*Client mode is a separate application in `client_app`

V4L2 example:
```
mode: v4l2
capture_device: /dev/video0
border_size: 80
vertical_leds: 23
horizontal_leds: 41
capture_width: 1280
capture_height: 720
capture_fps: 60
serial_port: /dev/serial/by-id/usb-Raspberry_Pi_Pico_E6614C311B593734-if00
baud: 921600
gamma_correction: 2.2
v4l2_buffer_count: 4
```

Network example:
```
mode: network
serial_port: /dev/serial/by-id/usb-Raspberry_Pi_Pico_E6614C311B593734-if00
baud: 921600
port: 8080
```

Client example:
```
border_size: 80
vertical_leds: 23
horizontal_leds: 41
gamma_correction: 2.2
server_ip: 127.0.0.1
server_port: 8888
```

## Serial/network protocol
The MCU serial communication and network mode communication protocols are identical.

Every message is `x*3+1` bytes long, where `x` is the number of LEDs, and the last byte is `\n`.

The red subpixel brightness of nth LED is at index `n*3`, the green subpixel brightness is at index `n*3+1`, and the blue subpixel brightness is at index `n*3+2`.

`\n`/`0x0A`/`10` value can only appear at the end of the message, not as a brightness value (ie. replace any `10` value with `9` or `11` before sending)
