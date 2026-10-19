"""The moshviz network wiring: the single source of truth for how operators connect.

Apply from the Textport (Dialogs -> Textport and DATs):

    exec(open(project.folder + '/scripts/wiring.py').read())

For every operator below, it compares the wired inputs and the operator
references (Feedback targets, Select sources, GLSL extra TOPs) with this
file, prints each difference it finds, and fixes it. Run it whenever the
network misbehaves: a wire accidentally dropped onto the wrong node, for
example, creates a cook loop that turns everything black.

Paths in WIRED and PARS are relative to NETWORK.
"""

NETWORK = '/project1'

# Wired inputs, in input order. A GLSL TOP takes at most three wired inputs.
WIRED = {
    'cam': ['mp_video'],
    'prev_cam': ['cam'],
    'motion': ['cam', 'prev_cam'],
    'stylize': ['cam', 'motion', 'mask'],
    'mosh_fb': ['stylize'],
    'mosh': ['stylize', 'mosh_fb', 'mask'],
    'look': ['mosh', 'motion'],
    'out': ['look'],
}

# Parameters that refer to other operators.
PARS = {
    'mp_video': {'top': 'image_segmentation/original_video'},  # Select TOP
    'mask': {'top': 'image_segmentation/selfie_mask'},         # Select TOP
    'prev_cam': {'top': 'cam'},                                # Feedback TOP target
    'mosh_fb': {'top': 'mosh'},                                # Feedback TOP target
    'mosh': {'tops': 'rows motion'},                           # GLSL inputs 3 and 4
    'rows': {'chop': 'rows_sel'},                              # CHOP to TOP source
}


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


def apply(network=NETWORK):
    _check_wiring(network)
    _check_pars(network)
    print('wiring: checked {} operators'.format(len(set(WIRED) | set(PARS))))


apply()
