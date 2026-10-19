"""MediaPipe plugin settings for moshviz: segmentation only, as a mask.

Apply from the Textport (Dialogs -> Textport and DATs):

    exec(open(project.folder + '/scripts/mediapipe_settings.py').read())

Plugin: torinmb/mediapipe-touchdesigner v0.6.0 (td/vendor/, gitignored).
Parameter names were read from the plugin's custom parameters on 19 Oct 2026.
The only segmentation model in this version is Selfie Multiclass. Keep it
in its normal colour-coded multiclass mode: the image_segmentation
component decodes that into separate masks, and moshviz uses its
`selfie_mask` (white = person) plus `original_video`, the frame the mask
was computed from. "Background only" mode sends a one-channel image that
image_segmentation cannot decode, so its outputs freeze (seen 19 Oct 2026).
"""

MEDIAPIPE = '/project1/MediaPipe'

SETTINGS = {
    # The only task we use: person segmentation for the subject mask.
    'Detectsegments': True,
    'Sshowmulticlassbackgroundonly': False,
    # Everything else costs CPU/GPU time and feeds nothing in moshviz.
    'Detectfacelandmarks': False,
    'Detectgestures': False,
    'Detectposes': False,
    'Detectobjects': False,
    'Detectfaces': False,
    'Detectimages': False,
    'Detectimageembeddings': False,
}


def apply(path=MEDIAPIPE, settings=SETTINGS):
    mediapipe = op(path)
    if mediapipe is None:
        print('mediapipe_settings: no operator at {}'.format(path))
        return
    for name, value in settings.items():
        par = getattr(mediapipe.par, name, None)
        if par is None:
            print('mediapipe_settings: {} has no parameter {}, skipped'.format(path, name))
            continue
        par.val = value
    print('mediapipe_settings: applied {} settings to {}'.format(len(settings), path))


apply()
