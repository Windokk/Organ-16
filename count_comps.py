import xml.etree.ElementTree as ET
from collections import Counter, defaultdict

def count_components_by_circuit(circ_file):
    try:
        tree = ET.parse(circ_file)
        root = tree.getroot()
    except ET.ParseError:
        print("❌ Error: Unable to parse the .circ file. Is it a valid Logisim file?")
        return
    except FileNotFoundError:
        print("❌ Error: File not found.")
        return

    # Dictionary: {circuit_name: Counter({component_name: count})}
    circuit_components = defaultdict(Counter)

    # Loop through each <circuit> element
    for circuit in root.findall('circuit'):
        circuit_name = circuit.attrib.get('name', 'Unnamed Circuit')

        # Find all <comp> tags inside this circuit
        for comp in circuit.iter('comp'):
            comp_name = comp.attrib.get('name', 'Unknown')
            circuit_components[circuit_name][comp_name] += 1

    # Print the output
    print(f"📦 Component Counts by Circuit in: {circ_file}\n")
    for circuit_name, components in circuit_components.items():
        print(f"🧩 Circuit: {circuit_name}")
        for comp_name, count in sorted(components.items()):
            print(f"  {comp_name}: {count}")
        print(f"  🔢 Total components: {sum(components.values())}\n")

if __name__ == "__main__":
    count_components_by_circuit("organ16.circ")
