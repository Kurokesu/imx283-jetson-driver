# IMX283 kernel driver for NVIDIA Jetson

![JetPack 6.2.1](https://img.shields.io/badge/JetPack_6.2.1-L4T_36.4.4-brightgreen?logo=nvidia&logoColor=white)
![JetPack 6.2.2](https://img.shields.io/badge/JetPack_6.2.2-L4T_36.5.0-brightgreen?logo=nvidia&logoColor=white)

NVIDIA Jetson kernel driver for Sony IMX283, a 20 MP 1" CMOS sensor.

- 4-lane MIPI CSI-2
- 12-bit RAW output
- 5472×3648 @ 10 fps

> [!NOTE]
> Only 720 Mbps MIPI lane rate is currently supported, which caps framerate at 10 fps.

## Setup

Install required tools:

```bash
sudo apt install -y --no-install-recommends dkms
```

Clone this repository:

```bash
cd ~
git clone https://github.com/Kurokesu/imx283-jetson-driver.git
cd imx283-jetson-driver/
```

Run setup script:

```bash
sudo ./setup.sh
```

Setup script:

- Fetches NVIDIA device tree headers required for build
- Builds and installs kernel module via [DKMS](https://github.com/dell/dkms)
- Builds and copies device tree overlay (`.dtbo`) to `/boot`

Use Jetson-IO to configure the CSI connector:

> [!NOTE]
> IMX283 requires 4-lane MIPI CSI, so only port C (`cam1`) is supported.

```bash
sudo /opt/nvidia/jetson-io/jetson-io.py
```

Navigate through the menu:

1. Configure Jetson CSI Connector (named "22pin" on 6.2.2, "24pin" on 6.2.1)
2. Configure for compatible hardware
3. Select `Camera IMX283-C`

![jetson-io-tool](./img/jetson-io-tool.png "jetson-io-tool")

4. Save pin changes
5. Save and reboot to reconfigure pins

After reboot, verify sensor is detected:

```bash
sudo dmesg | grep imx283
```

![dmesg-imx283](./img/dmesg.png "dmesg-imx283")

## Image output

Sensor active area is 5472×3648, but driver advertises 5568×3648 because sensor prepends 96 columns of horizontal optical black (HOB) on the left for ISP black-level reference. HOB columns appear as a dark stripe in the output unless explicitly cropped downstream.

### GStreamer

Cropped to active area (HOB columns removed via `nvvidconv`):

```bash
gst-launch-1.0 -e nvarguscamerasrc sensor-id=0 ! \
   'video/x-raw(memory:NVMM),width=5568,height=3648,framerate=10/1' ! \
   queue ! nvvidconv left=96 right=5568 top=0 bottom=3648 ! \
   'video/x-raw(memory:NVMM),width=5472,height=3648' ! \
   queue ! nveglglessink
```

Full sensor readout (active area + 96-column horizontal optical black):

```bash
gst-launch-1.0 -e nvarguscamerasrc sensor-id=0 ! \
   'video/x-raw(memory:NVMM),width=5568,height=3648,framerate=10/1' ! \
   queue ! nvvidconv ! queue ! nveglglessink
```

## Test mode

IMX283 has a built-in test pattern generator for verifying data validity.

Enable test pattern:

```bash
# Horizontal color-bar test pattern (test_mode = 5)
echo 5 | sudo tee /sys/module/nv_imx283/parameters/test_mode
```

Turn test pattern off:

```bash
echo 0 | sudo tee /sys/module/nv_imx283/parameters/test_mode
```

| Test pattern code | Description |
| ----------------- | ----------- |
| 0 | Off (normal operation) |
| 1 | All 000h |
| 2 | All FFFh |
| 3 | All 555h |
| 4 | All AAAh |
| 5 | Horizontal color bars |
| 6 | Vertical color bars |

## Development builds

For manual builds without DKMS:

```bash
make              # build everything (dtbo + kernel module)
sudo make install # copy dtbo to /boot, rmmod + insmod
```

> [!NOTE]
> Module is loaded immediately via `insmod` but won't persist across reboots. Use `sudo ./setup.sh` for permanent installation via DKMS.

Individual targets:

```bash
make dtbo      # build only the device tree overlay
make module    # build only the kernel module
make clean     # remove build artifacts
```

Build artifacts are placed in `./build`.
