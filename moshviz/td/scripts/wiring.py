"""The moshviz network wiring: the single source of truth for how operators connect.

Apply from the Textport (Dialogs -> Textport and DATs):

    exec(open(project.folder + '/scripts/wiring.py').read())

Movie clock (movie mode), MOSHVIZ_CLOCK:
  'audio'  (default) the movie follows the service's playback position, so
           the clip's own sound drives the visuals (service --file X --play).
  'free'   the movie loops by itself on TouchDesigner's wall clock, silently;
           the visuals react to whatever the service listens to (a mic, or
           computer audio via a loopback device such as BlackHole). Clip
           start/end on `controls` set the looped section.

    MOSHVIZ_CLOCK = 'free'; exec(open(project.folder + '/scripts/wiring.py').read())

To switch the input, set MOSHVIZ_SOURCE first (it stays set for later runs
in the same TouchDesigner session):

    MOSHVIZ_SOURCE = 'movie'; exec(open(project.folder + '/scripts/wiring.py').read())
    MOSHVIZ_SOURCE = 'camera'; exec(open(project.folder + '/scripts/wiring.py').read())

For every operator below, it compares the wired inputs and the operator
references (Feedback targets, Select sources, GLSL extra TOPs) with this
file, prints each difference it finds, and fixes it. Run it whenever the
network misbehaves: a wire accidentally dropped onto the wrong node, for
example, creates a cook loop that turns everything black.

SOURCE picks the input:
  'camera'  live webcam through MediaPipe, with the performer mask and clones.
  'movie'   a video file (MOVIE_FILE). MediaPipe on macOS only takes a webcam,
            so there is no performer mask: everyone moshes (the dance-floor
            case) and the clones pass is bypassed. The movie follows the
            audio clock: run the service with --file <same file> --play, and
            the movie shows the frame at /moshviz/time.

Control panel: click the `controls` node; its Controls page has
  Clones                 on/off switch for the clones pass (clones_switch)
  Clip start, Clip end   seconds; the service loops the audio between them
                         and the video follows (end 0 = file end)
  Zoom, Offset X/Y,      placement of the footage (movie mode)
  Rotate
The script creates these parameters once and never overwrites their values.
clip_timing, clip_xform and clones_switch read them through expressions.

Paths in WIRED and PARS are relative to NETWORK. Operators in CREATE are
made if missing (the others are built by hand, see WALKTHROUGH.md).
"""

NETWORK = '/project1'

SOURCE = globals().get('MOSHVIZ_SOURCE', 'camera')   # 'camera' or 'movie'
# Video for the movie node, relative to the .toe (recordings/ is gitignored).
# Use an intra-frame copy made with tools/intra_frame.py: the movie is
# seeked every frame to follow the audio clock, and HEVC phone footage
# stalls under that (it froze after a few seconds). The service still plays
# the audio from the original file.
MOVIE_FILE = '../recordings/IMG_1423_cfr30.mov'

# The cook rate must match the source's frame rate. Otherwise some frames
# repeat the previous image, the motion pass sees zero motion on them, and
# the mosh and glow flicker (measured: 43 uneven fps with a 30 fps camera
# and a 60 fps project).
CAMERA_FPS = 30.0
MOVIE_FPS = 30.0   # tools/intra_frame.py writes constant 30 fps (the phone clip is variable rate)
MOVIE_DURATION_S = 24.97   # IMG_1423_cfr30.mov, as printed by tools/intra_frame.py
# (Renamed from _prores.mov: TouchDesigner kept the replaced file's old
# 120 fps metadata cached under the same path.)

# Source resolution, so the motion pass keeps BLOCK_PX macroblocks whatever
# the input. A fixed 160 x 90 motion field gave 12 px blocks on 1920 x 1080
# footage, which made the mosh and glow visibly chunkier.
CAMERA_BLOCK_PX = 8
MOVIE_BLOCK_PX = 4   # finer for footage shown large; 8 px tiles read as pixelation
CAMERA_SIZE = (1280, 720)
MOVIE_SIZE = (720, 1280)   # IMG_1423.MOV, filmed upright: portrait after tools/intra_frame.py applies the rotation

HISTORY_FRAMES = 32
"""Frames of history kept for time-delayed clones (about 1 s at 30 fps)."""

CLOCK = globals().get('MOSHVIZ_CLOCK', 'audio')   # 'audio' or 'free' (movie mode only)
USE_MOVIE = SOURCE == 'movie'
FREE_CLOCK = USE_MOVIE and CLOCK == 'free'
VIDEO = 'clip_xform' if USE_MOVIE else 'mp_video'
SUBJECT_MASK = 'no_mask' if USE_MOVIE else 'mask'
# Clones need figures to copy. With no segmentation in movie mode, they
# copy whatever moves (shaders/motion_mask.glsl); mosh and stylize keep
# SUBJECT_MASK, so their behaviour is unchanged.
CLONE_MASK = 'motion_mask' if USE_MOVIE else 'mask'
SOURCE_SIZE = MOVIE_SIZE if USE_MOVIE else CAMERA_SIZE
BLOCK_PX = MOVIE_BLOCK_PX if USE_MOVIE else CAMERA_BLOCK_PX

# Operators the script creates if they do not exist: name -> operator type.
CREATE = {
    'history_image': 'texture3dTOP',
    'history_mask': 'texture3dTOP',
    'movie': 'moviefileinTOP',
    'no_mask': 'constantTOP',
    'clip_xform': 'transformTOP',
    'clip_timing': 'constantCHOP',
    'clip_osc': 'oscoutCHOP',
    'controls': 'baseCOMP',
    'clones_switch': 'switchTOP',
    'prep': 'glslTOP',          # input grading (shaders/prep.glsl), before all analysis
    'prep_pixel': 'textDAT',    # holds prep.glsl, synced to the file
    'motion_mask': 'glslTOP',   # "whatever moves" mask for clones (shaders/motion_mask.glsl)
    'motion_mask_pixel': 'textDAT',
}

# Custom parameters on `controls`: name -> (kind, label, default, slider range).
# TouchDesigner requires custom parameter names to start with a capital
# letter followed by lowercase letters or digits. The slider range only sets
# the slider's travel (default 0..1); typed values outside it still work.
CONTROLS = {
    'Clones': ('toggle', 'Clones', False, None),
    'Clipstart': ('float', 'Clip start (s)', 0.0, (0.0, 30.0)),
    'Clipend': ('float', 'Clip end (s, 0 = end)', 0.0, (0.0, 30.0)),
    'Zoom': ('float', 'Zoom', 1.0, (0.25, 4.0)),
    'Offsetx': ('float', 'Offset X', 0.0, (-1.0, 1.0)),
    'Offsety': ('float', 'Offset Y', 0.0, (-1.0, 1.0)),
    'Rotate': ('float', 'Rotate (deg)', 0.0, (-180.0, 180.0)),
    # Input grading (prep pass): applied before motion and outline analysis.
    'Exposure': ('float', 'Input exposure', 1.0, (0.0, 8.0)),
    'Brightness': ('float', 'Input brightness', 0.0, (-0.5, 0.5)),
    'Contrast': ('float', 'Input contrast', 1.0, (0.0, 3.0)),
    'Gamma': ('float', 'Input gamma', 1.0, (0.2, 4.0)),
    'Saturation': ('float', 'Input saturation', 1.0, (0.0, 2.0)),
    'Soften': ('float', 'Input soften (denoise)', 0.0, (0.0, 1.0)),
}

# Channels of clip_timing, sent to the service as OSC addresses.
CLIP_CHANNELS = ('moshviz/clip/start', 'moshviz/clip/end')

# Wired inputs, in input order. A GLSL TOP takes at most three wired inputs.
WIRED = {
    'prep': [VIDEO],
    'cam': ['prep'],
    'prev_cam': ['cam'],
    'motion': ['cam', 'prev_cam'],
    'stylize': ['cam', 'motion', SUBJECT_MASK],
    'mosh_fb': ['stylize'],
    'mosh': ['stylize', 'mosh_fb', SUBJECT_MASK],
    'look': ['mosh', 'motion'],
    'history_image': ['look'],
    'history_mask': [CLONE_MASK],
    'motion_mask': ['motion'],
    'clones': ['look', 'history_image', 'history_mask'],
    'clones_switch': ['look', 'clones'],   # index 0 = clones off, 1 = on
    'out': ['clones_switch'],
    'clip_xform': ['movie'],
    'clip_osc': ['clip_timing'],
}

# Parameters that refer to other operators.
PARS = {
    'mp_video': {'top': 'image_segmentation/original_video'},  # Select TOP
    'mask': {'top': 'image_segmentation/selfie_mask'},         # Select TOP
    'prev_cam': {'top': 'cam'},                                # Feedback TOP target
    'mosh_fb': {'top': 'mosh'},                                # Feedback TOP target
    'mosh': {'tops': 'rows motion'},                           # GLSL inputs 3 and 4
    'clones': {'tops': CLONE_MASK},                            # live mask, input 3
    'rows': {'chop': 'rows_sel'},                              # CHOP to TOP source
    'prep': {'pixeldat': 'prep_pixel'},                        # GLSL TOP shader source
    'motion_mask': {'pixeldat': 'motion_mask_pixel'},          # GLSL TOP shader source
}

# Constant settings of created operators.
HISTORY_SETTINGS = {
    'type': 'texture2darray',
    'cachesize': HISTORY_FRAMES,
    'replacesingle': True,
}
SETTINGS = {
    'history_image': HISTORY_SETTINGS,
    'history_mask': HISTORY_SETTINGS,
    # The movie always shows the frame at a time in seconds (see EXPRESSIONS):
    # the audio playback position, or the wall clock looped over the clip.
    # (Sequential play mode advances one frame per cook, and this network
    # can cook the movie more than once per frame, so it played too fast.)
    'movie': {'file': MOVIE_FILE, 'playmode': 'specify', 'indexunit': 'seconds'},
    'no_mask': {'colorr': 0.0, 'colorg': 0.0, 'colorb': 0.0, 'alpha': 1.0},
    'prep_pixel': {'file': 'shaders/prep.glsl', 'syncfile': True},
    'prep': {'format': 'rgba16float'},
    'motion_mask_pixel': {'file': 'shaders/motion_mask.glsl', 'syncfile': True},
    'motion_mask': {'format': 'rgba16float', 'outputresolution': 'custom',
                    'resolutionw': SOURCE_SIZE[0], 'resolutionh': SOURCE_SIZE[1]},
    'clip_osc': {'netaddress': '127.0.0.1', 'port': 9001, 'sendevents': True},
    'motion': {'outputresolution': 'custom',
               'resolutionw': SOURCE_SIZE[0] // BLOCK_PX,
               'resolutionh': SOURCE_SIZE[1] // BLOCK_PX},
}

# Expression settings of created operators.
#  - History nodes are ring buffers whose write slot is set explicitly to
#    frame mod HISTORY_FRAMES, so clones.glsl can find the frame from N frames ago.
#  - The movie shows the frame at the audio playback position (0 until the
#    service sends it).
_TIME = "op('oscin1')['moshviz/time']"
HISTORY_EXPRESSIONS = {'replaceindex': 'absTime.frame % {}'.format(HISTORY_FRAMES)}
_CONTROL = "op('controls').par."
# Wall clock looped over [Clip start, Clip end) (end 0 = clip length); the
# loop length is kept above one frame so it never divides by zero.
_FREE_INDEX = ("{c}Clipstart.eval() + absTime.seconds % "
               "max(({c}Clipend.eval() or {d}) - {c}Clipstart.eval(), {f})"
               .format(c="op('controls').par.", d=MOVIE_DURATION_S, f=1.0 / MOVIE_FPS))
EXPRESSIONS = {
    'history_image': HISTORY_EXPRESSIONS,
    'history_mask': HISTORY_EXPRESSIONS,
    'movie': {'index': (_FREE_INDEX if FREE_CLOCK else
                        '{0}.eval() if {0} is not None else 0'.format(_TIME))},
    'clip_timing': {'const0value': _CONTROL + 'Clipstart',
                    'const1value': _CONTROL + 'Clipend'},
    'clip_xform': {'sx': _CONTROL + 'Zoom', 'sy': _CONTROL + 'Zoom',
                   'tx': _CONTROL + 'Offsetx', 'ty': _CONTROL + 'Offsety',
                   'rotate': _CONTROL + 'Rotate'},
    'clones_switch': {'index': 'int(' + _CONTROL + 'Clones)'},
}

# Clones are switched by clones_switch now; make sure the old bypass is off.
BYPASSED = {'clones': False}


def _create_missing(network):
    parent = op(network)
    for name, op_type in CREATE.items():
        if op(network + '/' + name) is None:
            print('wiring: creating {} ({})'.format(name, op_type))
            parent.create(eval(op_type), name)  # e.g. texture3dTOP, a TouchDesigner built-in


def _check_settings(network):
    for name, settings in SETTINGS.items():
        node = op(network + '/' + name)
        if node is None:
            continue
        for par_name, value in settings.items():
            par = getattr(node.par, par_name, None)
            if par is None:
                print('wiring: {} has no parameter {}'.format(name, par_name))
            elif par.eval() != value:
                print('wiring: {}.{} {!r} -> {!r}'.format(name, par_name, par.eval(), value))
                par.mode = ParMode.CONSTANT
                par.val = value
    for name, expressions in EXPRESSIONS.items():
        node = op(network + '/' + name)
        if node is None:
            continue
        for par_name, expression in expressions.items():
            par = getattr(node.par, par_name, None)
            if par is None:
                print('wiring: {} has no parameter {}'.format(name, par_name))
            elif par.expr != expression or par.mode != ParMode.EXPRESSION:
                print('wiring: {}.{} = {}'.format(name, par_name, expression))
                par.expr = expression
                par.mode = ParMode.EXPRESSION


def _check_wiring(network):
    for name, input_names in WIRED.items():
        node = op(network + '/' + name)
        if node is None:
            print('wiring: missing operator {}'.format(name))
            continue
        current = [i.name if i is not None else None for i in node.inputs]
        if current != input_names:
            print('wiring: {} inputs {} -> {}'.format(name, current, input_names))
            node.setInputs([op(network + '/' + n) for n in input_names])


def _check_pars(network):
    for name, pars in PARS.items():
        node = op(network + '/' + name)
        if node is None:
            print('wiring: missing operator {}'.format(name))
            continue
        for par_name, value in pars.items():
            par = getattr(node.par, par_name, None)
            if par is None:
                print('wiring: {} has no parameter {}'.format(name, par_name))
                continue
            # .val is the typed text; .eval() would return an OP object for reference parameters.
            if str(par.val) != value:
                print('wiring: {}.{} {!r} -> {!r}'.format(name, par_name, par.val, value))
                par.mode = ParMode.CONSTANT
                par.val = value


def _check_bypass(network):
    for name, bypassed in BYPASSED.items():
        node = op(network + '/' + name)
        if node is not None and node.bypass != bypassed:
            print('wiring: {} bypass -> {}'.format(name, bypassed))
            node.bypass = bypassed


def _check_clip_timing(network):
    """Give clip_timing its channel names. Values are left alone: they are yours to edit."""
    node = op(network + '/clip_timing')
    if node is None:
        return
    if node.seq.const.numBlocks < len(CLIP_CHANNELS):
        node.seq.const.numBlocks = len(CLIP_CHANNELS)
    for i, name in enumerate(CLIP_CHANNELS):
        par = getattr(node.par, 'const{}name'.format(i), None)
        if par is None:
            print('wiring: clip_timing has no parameter const{}name'.format(i))
        elif par.eval() != name:
            print('wiring: clip_timing channel {} -> {}'.format(i, name))
            par.val = name


def _check_controls(network):
    """Create the control panel's custom parameters once; never touch their values."""
    controls = op(network + '/controls')
    if controls is None:
        return
    page = next((p for p in controls.customPages if p.name == 'Controls'), None)
    if page is None:
        page = controls.appendCustomPage('Controls')
    for name, (kind, label, default, slider) in CONTROLS.items():
        par = getattr(controls.par, name, None)
        if par is None:
            print('wiring: controls gets {}'.format(label))
            if kind == 'toggle':
                par = page.appendToggle(name, label=label)[0]
            else:
                par = page.appendFloat(name, label=label)[0]
            par.default = default
            par.val = default
        if slider is not None and (par.normMin, par.normMax) != slider:
            par.normMin, par.normMax = slider   # slider travel only, not the value


def _check_cook_rate():
    rate = MOVIE_FPS if USE_MOVIE else CAMERA_FPS
    if abs(project.cookRate - rate) > 1e-3:
        print('wiring: cook rate {} -> {}'.format(project.cookRate, rate))
        project.cookRate = rate


def apply(network=NETWORK):
    if SOURCE not in ('camera', 'movie'):
        print("wiring: MOSHVIZ_SOURCE must be 'camera' or 'movie', not {!r}".format(SOURCE))
        return
    if CLOCK not in ('audio', 'free'):
        print("wiring: MOSHVIZ_CLOCK must be 'audio' or 'free', not {!r}".format(CLOCK))
        return
    _check_cook_rate()
    _create_missing(network)
    _check_controls(network)
    _check_wiring(network)
    _check_pars(network)
    _check_clip_timing(network)   # rows must exist before their expressions are set
    _check_settings(network)
    _check_bypass(network)
    print('wiring: source {}{}, checked {} operators'.format(
        SOURCE, ' (clock {})'.format(CLOCK) if USE_MOVIE else '',
        len(set(WIRED) | set(PARS) | set(CREATE))))


apply()
