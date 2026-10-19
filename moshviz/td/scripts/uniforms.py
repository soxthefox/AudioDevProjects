"""Shader uniform values for the moshviz GLSL TOPs: the single source of truth.

The .toe is binary, so values typed into the Vectors page are invisible to
version control. Keep them here instead and apply them from the Textport
(Dialogs -> Textport and DATs):

    exec(open(project.folder + '/scripts/uniforms.py').read())

That sets every uniform below on its GLSL TOP, finding each row by its
uniform name and adding a row if the name is missing. Run it again after
editing this file. Values are tuples of 1 to 4 numbers (x, y, z, w);
strings are Python expressions (for example 'absTime.frame').
"""

NETWORK = '/project1'

UNIFORMS = {
    'motion': {
        'uRegularisation': (0.001,),
    },
    'mosh': {
        'uDisplacement': (3.0,),
        'uMaxMotionPx': (16.0,),
        'uRefresh': (0.05,),
        'uMaskFeather': (0.1,),
        'uThresholdSoftness': (0.05,),
        'uReseedFrames': (0.0,),
        'uFrame': 'absTime.frame',
        'uRadial': (1.0,),
        'uRadialCurve': (3.0,),
    },
    'look': {
        'uBrightness': (-0.1,),
        'uContrast': (1.3,),
        'uSaturation': (0.6,),
        'uDuotone': (0.0,),
        'uShadowTint': (0.05, 0.0, 0.15),
        'uHighlightTint': (1.0, 0.75, 0.6),
        'uMotionGlow': (0.3,),
        'uGlowAudio': (1.5,),
        'uMotionScalePx': (3.0,),
        'uMotionFloorPx': (0.3,),
    },
}

COMPONENTS = 'xyzw'


def _row_index(glsl, name):
    """Index of the Vectors row holding `name`, adding a row if there is none."""
    vectors = glsl.seq.vec
    for i in range(vectors.numBlocks):
        if glsl.par['vec{}name'.format(i)].eval() == name:
            return i
    vectors.numBlocks += 1
    i = vectors.numBlocks - 1
    glsl.par['vec{}name'.format(i)].val = name
    return i


def apply(network=NETWORK, uniforms=UNIFORMS):
    for op_name, values in uniforms.items():
        glsl = op(network + '/' + op_name)
        if glsl is None:
            print('uniforms: no operator {}/{}, skipped'.format(network, op_name))
            continue
        for name, value in values.items():
            i = _row_index(glsl, name)
            if isinstance(value, str):
                par = glsl.par['vec{}valuex'.format(i)]
                par.expr = value
                par.mode = ParMode.EXPRESSION
                continue
            for component, number in zip(COMPONENTS, value):
                par = glsl.par['vec{}value{}'.format(i, component)]
                par.mode = ParMode.CONSTANT
                par.val = number
        print('uniforms: set {} on {}'.format(len(values), glsl.path))


apply()
