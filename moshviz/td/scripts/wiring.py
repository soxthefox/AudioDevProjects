"""The moshviz network wiring: the single source of truth for how operators connect.

Apply from the Textport (Dialogs -> Textport and DATs):

    exec(open(project.folder + '/scripts/wiring.py').read())

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

Clip controls (movie mode), edited by hand in TouchDesigner:
  clip_xform   Transform TOP between the movie and `cam`: zoom, position and
               rotation of the footage. Bypass it to reset.
  clip_timing  Constant CHOP with moshviz/clip/start and moshviz/clip/end, in
               seconds (end 0 = file end). clip_osc sends them to the service,
               which loops the audio between them, and the video follows.

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
MOVIE_FILE = '../recordings/IMG_1422_2_prores.mov'

# The cook rate must match the source's frame rate. Otherwise some frames
# repeat the previous image, the motion pass sees zero motion on them, and
# the mosh and glow flicker (measured: 43 uneven fps with a 30 fps camera
# and a 60 fps project).
CAMERA_FPS = 30.0
MOVIE_FPS = 23.879   # IMG_1422_2.MOV average frame rate, read with PyAV

# Source resolution, so the motion pass keeps BLOCK_PX macroblocks whatever
# the input. A fixed 160 x 90 motion field gave 12 px blocks on 1920 x 1080
# footage, which made the mosh and glow visibly chunkier.
BLOCK_PX = 8
CAMERA_SIZE = (1280, 720)
MOVIE_SIZE = (1280, 720)   # IMG_1422_2.MOV is 1920 x 1080, but the Non-Commercial licence caps TOPs at 1280

HISTORY_FRAMES = 32
"""Frames of history kept for time-delayed clones (about 1 s at 30 fps)."""

USE_MOVIE = SOURCE == 'movie'
VIDEO = 'clip_xform' if USE_MOVIE else 'mp_video'
SUBJECT_MASK = 'no_mask' if USE_MOVIE else 'mask'
SOURCE_SIZE = MOVIE_SIZE if USE_MOVIE else CAMERA_SIZE

# Operators the script creates if they do not exist: name -> operator type.
CREATE = {
    'history_image': 'texture3dTOP',
    'history_mask': 'texture3dTOP',
    'movie': 'moviefileinTOP',
    'no_mask': 'constantTOP',
    'clip_xform': 'transformTOP',
    'clip_timing': 'constantCHOP',
    'clip_osc': 'oscoutCHOP',
}

# Channels of clip_timing, sent to the service as OSC addresses.
CLIP_CHANNELS = ('moshviz/clip/start', 'moshviz/clip/end')

# Wired inputs, in input order. A GLSL TOP takes at most three wired inputs.
WIRED = {
    'cam': [VIDEO],
    'prev_cam': ['cam'],
    'motion': ['cam', 'prev_cam'],
    'stylize': ['cam', 'motion', SUBJECT_MASK],
    'mosh_fb': ['stylize'],
    'mosh': ['stylize', 'mosh_fb', SUBJECT_MASK],
    'look': ['mosh', 'motion'],
    'history_image': ['look'],
    'history_mask': [SUBJECT_MASK],
    'clones': ['look', 'history_image', 'history_mask'],
    'out': ['clones'],
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
    'clones': {'tops': SUBJECT_MASK},                          # live mask, input 3
    'rows': {'chop': 'rows_sel'},                              # CHOP to TOP source
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
    'movie': {'file': MOVIE_FILE, 'playmode': 'specify', 'indexunit': 'seconds'},
    'no_mask': {'colorr': 0.0, 'colorg': 0.0, 'colorb': 0.0, 'alpha': 1.0},
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
EXPRESSIONS = {
    'history_image': HISTORY_EXPRESSIONS,
    'history_mask': HISTORY_EXPRESSIONS,
    'movie': {'index': '{0}.eval() if {0} is not None else 0'.format(_TIME)},
}

# Operators bypassed in the current SOURCE mode (bypassed TOPs pass input 0 through).
BYPASSED = {'clones': USE_MOVIE}


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


def _check_cook_rate():
    rate = MOVIE_FPS if USE_MOVIE else CAMERA_FPS
    if abs(project.cookRate - rate) > 1e-3:
        print('wiring: cook rate {} -> {}'.format(project.cookRate, rate))
        project.cookRate = rate


def apply(network=NETWORK):
    if SOURCE not in ('camera', 'movie'):
        print("wiring: MOSHVIZ_SOURCE must be 'camera' or 'movie', not {!r}".format(SOURCE))
        return
    _check_cook_rate()
    _create_missing(network)
    _check_wiring(network)
    _check_pars(network)
    _check_settings(network)
    _check_clip_timing(network)
    _check_bypass(network)
    print('wiring: source {}, checked {} operators'.format(
        SOURCE, len(set(WIRED) | set(PARS) | set(CREATE))))


apply()
