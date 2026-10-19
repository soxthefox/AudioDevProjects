# Camera and subject mask (milestones 1 and 2)

Not started. This needs hardware and a Homebrew install, so it waits for
Sohum.

## Spike plan: Kinect v1 on Apple Silicon (time box: 1 day)

1. `brew install libfreenect` (needs approval; it also pulls in libusb).
2. Plug in the Kinect with its separate power adapter, then run
   `freenect-glview`. Success means live depth and RGB windows.
   - If the device does not enumerate, check `system_profiler SPUSBDataType`
     for "Xbox NUI Motor/Camera/Audio". On some Macs, Kinect v1 only works
     through a powered USB 2 hub.
3. Bridge into TouchDesigner. Candidates, in order of preference:
   - **Syphon**: native GPU texture sharing on macOS, received by
     TouchDesigner's Syphon Spout In TOP. The sender would be a small
     Python (freenect bindings + `syphon-python`) or C program.
   - **NDI**: more setup and adds compression latency, but it works over a
     network.
   - **Raw frames over a local socket into a Script TOP**: no extra
     dependencies, but a copy on the CPU every frame.
   Measure the latency and the frame rate of the chosen option, and log it
   in docs/decisions.md (D3).
4. Mask: in TouchDesigner, depth → Threshold TOP at the performer's
   distance band → Blur → Threshold again (this works like a soft erode)
   → mask input of `mosh`.

## Current: webcam + MediaPipe plugin

In use since 19 Oct 2026, ahead of the Kinect spike. The plugin is
[torinmb/mediapipe-touchdesigner](https://github.com/torinmb/mediapipe-touchdesigner)
v0.6.0. Download `release.zip` from its releases page and unzip it to
`td/vendor/mediapipe-touchdesigner/`, which is gitignored (188 MB). It
runs MediaPipe segmentation on the GPU inside TouchDesigner. Its mask
(`seg_data`) replaces `mask_stub` as `mosh` input 2, and its video output
feeds `cam`, so the mask and the picture come from the same frames.

## Original fallback note: webcam + MediaPipe

Switch to this if step 2 fails or the spike passes one day. A MediaPipe
plugin for TouchDesigner exists (it runs MediaPipe inside a Web Render
TOP, so it is not tied to Nvidia). Its segmentation output goes straight
into the mosh shader's mask input. Nothing downstream changes, because the
shader only sees "a white-is-subject texture".
