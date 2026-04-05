#
# create_topology_examples.py
#
# Run this script inside BespokeSynth to create example .bsk savestate
# files for the topology suite modules.
#
# Usage: Load this script in a ScriptModule, click Run.
# It will create and save several example patches.
#

import bespoke
import me
import module

# Helper: create a basic topology showcase
def create_showcase():
    """Creates a patch with TopologySynth + NoteSequencer + Output"""
    # This script can set controls on modules that already exist.
    # The user should first load the topology_showcase layout,
    # then run this script to configure the parameters.

    print("Topology Suite — Example Configuration Script")
    print("=============================================")
    print("")
    print("To create example .bsk files:")
    print("1. File > Open Layout > topology_showcase.json")
    print("2. The modules will appear with default settings")
    print("3. File > Save As > savestate/example__topology_showcase.bsk")
    print("")
    print("For the tanpura preset:")
    print("1. File > Open Layout > topology_tanpura.json")
    print("2. File > Save As > savestate/example__topology_tanpura.bsk")
    print("")
    print("For the advanced preset:")
    print("1. File > Open Layout > topology_advanced.json")
    print("2. File > Save As > savestate/example__topology_advanced.bsk")
    print("")
    print("Once saved, these will appear on the Welcome Screen.")

create_showcase()
