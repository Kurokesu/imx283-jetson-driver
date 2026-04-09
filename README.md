# IMX283 MIPI NVIDIA driver

NVIDIA Jetson kernel driver for Sony IMX283 — a 20 MP 1" CMOS sensor.

- 4-lane MIPI CSI-2
- 12-bit RAW output
- 5472×3648 @ 20 fps

> [!NOTE]
> Currently, only `cam0` port support is implemented.

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
- Builds and installs kernel module via DKMS
- Builds and copies device tree overlay (`.dtbo`) to `/boot`

Use Jetson-IO to configure the CSI connector:

```bash
sudo /opt/nvidia/jetson-io/jetson-io.py
```

Navigate through the menu:

1. Configure Jetson CSI Connector (named "22pin" on 6.2.2, "24pin" on 6.2.1)
2. Configure for compatible hardware
3. Select Camera IMX283-A
4. Save pin changes
5. Save and reboot to reconfigure pins

After reboot, verify sensor is detected:

```bash
sudo dmesg | grep imx283
```

## Image output

### GStreamer

```bash
gst-launch-1.0 -e nvarguscamerasrc sensor-id=0 ! \
   'video/x-raw(memory:NVMM),width=5472,height=3648,framerate=20/1' ! \
   queue ! nvvidconv ! queue ! nveglglessink
```

### NVIDIA sample camera capture application

```bash
nvgstcapture-1.0 --sensor-id 0
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

| Test pattern code | Description          |
| ----------------- | -------------------- |
| 0                 | Off (normal)         |
| 1                 | All 000h             |
| 2                 | All FFFh             |
| 3                 | All 555h             |
| 4                 | All AAAh             |
| 5                 | Horizontal color bar |
| 6                 | Vertical color bar   |

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
