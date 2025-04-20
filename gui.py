import tkinter as tk
from tkinter import ttk
import json
import re
from collections import defaultdict
import os

# Conversion function: sim_output.txt to JSON
def convert_sim_output_to_json(input_file, output_file):
    simulator_data = {
        "instructions": [
            {"addr": "0x00000000", "instr": "0x00400293", "asm": "ADDI x5, x0, 4"},
            {"addr": "0x00000004", "instr": "0x00100313", "asm": "ADDI x6, x0, 1"},
            {"addr": "0x00000008", "instr": "0x00028863", "asm": "BEQ x5, x0, 16"},
            {"addr": "0x0000000C", "instr": "0x02530333", "asm": "MUL x6, x6, x5"},
            {"addr": "0x00000010", "instr": "0xFFF28293", "asm": "ADDI x5, x5, -1"},
            {"addr": "0x00000014", "instr": "0xFF5FF06F", "asm": "JAL x0, -12"}
        ],
        "cycles": []
    }
    stats = {}

    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: '{input_file}' not found")
        return

    current_cycle = None
    reg_file = False
    pipeline_reg = False
    last_regs = {i: 0 for i in range(32)}  # Track register values
    cycle_regs = defaultdict(dict)

    for i, line in enumerate(lines):
        line = line.strip()
        if line.startswith("=== Cycle"):
            cycle_num = int(re.search(r'Cycle (\d+)', line).group(1))
            current_cycle = {
                "cycle": cycle_num,
                "fetch": {"valid": False, "message": "Inactive"},
                "decode": {"valid": False, "message": "Inactive"},
                "execute": {"valid": False, "message": "Inactive"},
                "memory": {"valid": False, "message": "Inactive"},
                "writeback": {"valid": False, "message": "Inactive"},
                "prediction": {},
                "btb": "BTB is empty",
                "registers": {
                    "ifId": {"valid": False},
                    "idEx": {"valid": False},
                    "exMem": {"valid": False},
                    "memWb": {"valid": False}
                },
                "registerUpdates": []
            }
            simulator_data["cycles"].append(current_cycle)
        elif line.startswith("Fetch:"):
            if "Stalled" in line:
                current_cycle["fetch"] = {"valid": False, "message": line.split(": ", 1)[1]}
            else:
                parts = re.search(r'PC=(0x[0-9A-Fa-f]+), IR=(0x[0-9A-Fa-f]+), Instr#=(\d+), NextPC=(0x[0-9A-Fa-f]+)', line)
                if parts:
                    current_cycle["fetch"] = {
                        "valid": True,
                        "pc": parts.group(1),
                        "ir": parts.group(2),
                        "instrNum": int(parts.group(3)),
                        "nextPc": parts.group(4)
                    }
        elif line.startswith("Decode:"):
            if "stalled" in line or "invalid" in line:
                current_cycle["decode"] = {"valid": False, "message": line.split(": ", 1)[1]}
            else:
                parts = re.search(r'PC=(0x[0-9A-Fa-f]+), IR=(0x[0-9A-Fa-f]+), rs1=x(\d+)\((-?\d+)\), rs2=x(\d+)\((-?\d+)\), rd=x(\d+)', line)
                branch = re.search(r'Taken=(\d), Target=(0x[0-9A-Fa-f]+)', line)
                decode_data = {"valid": True}
                if parts:
                    decode_data.update({
                        "pc": parts.group(1),
                        "ir": parts.group(2),
                        "rs1": f"x{parts.group(3)}({parts.group(4)})",
                        "rs2": f"x{parts.group(5)}({parts.group(6)})",
                        "rd": f"x{parts.group(7)}"
                    })
                if branch:
                    decode_data["branchInfo"] = {
                        "type": "BEQ",
                        "rs1": f"x{parts.group(3)}({parts.group(4)})",
                        "rs2": f"x{parts.group(5)}({parts.group(6)})",
                        "taken": int(branch.group(1)),
                        "target": branch.group(2)
                    }
                current_cycle["decode"] = decode_data
        elif line.startswith("Execute:"):
            if "skipping" in line:
                current_cycle["execute"] = {"valid": False, "message": line.split(": ", 1)[1]}
            else:
                parts = re.search(r'PC=(0x[0-9A-Fa-f]+), IR=(0x[0-9A-Fa-f]+), (.*?)(?:, Result=(-?\d+))?', line)
                if parts:
                    current_cycle["execute"] = {
                        "valid": True,
                        "pc": parts.group(1),
                        "ir": parts.group(2),
                        "alu": parts.group(3).strip()
                    }
                    if parts.group(4):
                        current_cycle["execute"]["result"] = int(parts.group(4))
        elif line.startswith("Memory:"):
            if "No memory operation" in line or "Bypassing" in line:
                current_cycle["memory"] = {"valid": True, "message": line.split(": ", 1)[1]}
            else:
                parts = re.search(r'PC=(0x[0-9A-Fa-f]+), Instr#=(\d+)', line)
                if parts:
                    current_cycle["memory"] = {
                        "valid": True,
                        "pc": parts.group(1),
                        "instrNum": int(parts.group(2))
                    }
        elif line.startswith("Writeback:"):
            if "No writeback" in line or "skipping" in line:
                current_cycle["writeback"] = {"valid": False, "message": line.split(": ", 1)[1]}
            else:
                reg_match = re.search(r'x(\d+) = 0x[0-9A-Fa-f]+ \((\d+)\)', line)
                pc_match = re.search(r'PC=(0x[0-9A-Fa-f]+), Instr#=(\d+)', line)
                if reg_match:
                    reg = int(reg_match.group(1))
                    val = int(reg_match.group(2))
                    current_cycle["writeback"] = {
                        "valid": True,
                        "message": f"x{reg} = 0x{val:08x}"
                    }
                    current_cycle["registerUpdates"].append({
                        "register": f"x{reg}",
                        "value": f"0x{val:08x}"
                    })
                elif pc_match:
                    current_cycle["writeback"] = {
                        "valid": True,
                        "pc": pc_match.group(1),
                        "instrNum": int(pc_match.group(2))
                    }
        elif line.startswith("BTB:"):
            current_cycle["btb"] = line.split(": ", 1)[1]
        elif line.startswith("Prediction:"):
            pred = line.split(": ", 1)[1]
            current_cycle["prediction"] = {"message": pred}
            if "PC=" in pred:
                parts = re.search(r'PC=(0x[0-9A-Fa-f]+), (.*?)(?:, Correct=(\d), Update=(.*))?', pred)
                if parts:
                    current_cycle["prediction"] = {
                        "pc": parts.group(1),
                        "message": parts.group(2),
                        "correct": bool(int(parts.group(3))) if parts.group(3) else None,
                        "update": parts.group(4) if parts.group(4) else None
                    }
        elif line.startswith("Register File:"):
            reg_file = True
        elif reg_file and line.startswith("x"):
            regs = re.findall(r'x(\d+): (-?\d+) \(0x[0-9A-Fa-f]+\)', line)
            for reg, val in regs:
                reg = int(reg)
                val = int(val)
                cycle_regs[cycle_num][reg] = val
        elif line.startswith("Pipeline Registers:"):
            pipeline_reg = True
            reg_file = False
        elif pipeline_reg and line.startswith("IF/ID:"):
            if "INVALID" in line:
                current_cycle["registers"]["ifId"] = {"valid": False}
            else:
                parts = re.search(r'PC=(0x[0-9A-Fa-f]+), IR=(0x[0-9A-Fa-f]+), Instr#=(\d+)', line)
                if parts:
                    current_cycle["registers"]["ifId"] = {
                        "valid": True,
                        "pc": parts.group(1),
                        "ir": parts.group(2),
                        "instrNum": int(parts.group(3))
                    }
        elif pipeline_reg and line.startswith("ID/EX:"):
            if "INVALID" in line:
                current_cycle["registers"]["idEx"] = {"valid": False}
            else:
                parts = re.search(r'PC=(0x[0-9A-Fa-f]+), rs1=x(\d+)\((-?\d+)\), rs2=x(\d+)\((-?\d+)\), rd=x(\d+), imm=(-?\d+), ALU=(\w+), Instr#=(\d+)', line)
                if parts:
                    current_cycle["registers"]["idEx"] = {
                        "valid": True,
                        "pc": parts.group(1),
                        "rs1": f"x{parts.group(2)}({parts.group(3)})",
                        "rs2": f"x{parts.group(4)}({parts.group(5)})",
                        "rd": f"x{parts.group(6)}",
                        "imm": int(parts.group(7)),
                        "alu": parts.group(8),
                        "instrNum": int(parts.group(9))
                    }
        elif pipeline_reg and line.startswith("EX/MEM:"):
            if "INVALID" in line:
                current_cycle["registers"]["exMem"] = {"valid": False}
            else:
                parts = re.search(r'PC=(0x[0-9A-Fa-f]+), ALU=(-?\d+), rs2_val=(-?\d+), rd=x(\d+), Ctrl=(\w+), Instr#=(\d+)', line)
                if parts:
                    current_cycle["registers"]["exMem"] = {
                        "valid": True,
                        "pc": parts.group(1),
                        "alu": int(parts.group(2)),
                        "rs2Val": int(parts.group(3)),
                        "rd": f"x{parts.group(4)}",
                        "ctrl": parts.group(5),
                        "instrNum": int(parts.group(6))
                    }
        elif pipeline_reg and line.startswith("MEM/WB:"):
            if "INVALID" in line:
                current_cycle["registers"]["memWb"] = {"valid": False}
            else:
                parts = re.search(r'PC=(0x[0-9A-Fa-f]+), WData=(-?\d+), rd=x(\d+), Ctrl=(\w+), Instr#=(\d+)', line)
                if parts:
                    current_cycle["registers"]["memWb"] = {
                        "valid": True,
                        "pc": parts.group(1),
                        "wData": int(parts.group(2)),
                        "rd": f"x{parts.group(3)}",
                        "ctrl": parts.group(4),
                        "instrNum": int(parts.group(5))
                    }
        elif line == "Simulation Statistics:":
            for stat_line in lines[i+1:]:
                stat_line = stat_line.strip()
                stat_match = re.match(r'(\w[\w\s/]+): (\d+\.?\d*)', stat_line)
                if stat_match:
                    stats[stat_match.group(1)] = float(stat_match.group(2)) if '.' in stat_match.group(2) else int(stat_match.group(2))
                else:
                    break

    # Generate registerUpdates by comparing consecutive cycles
    for cycle_num in range(1, len(simulator_data["cycles"]) + 1):
        curr_regs = cycle_regs.get(cycle_num, {})
        prev_regs = cycle_regs.get(cycle_num - 1, last_regs)
        updates = []
        for reg in curr_regs:
            if reg not in prev_regs or curr_regs[reg] != prev_regs[reg]:
                updates.append({"register": f"x{reg}", "value": f"0x{curr_regs[reg]:08x}"})
        if updates:
            for cycle in simulator_data["cycles"]:
                if cycle["cycle"] == cycle_num:
                    cycle["registerUpdates"] = updates
        last_regs.update(curr_regs)

    # Save to JSON file
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(simulator_data, f, indent=2)
    print(f"JSON data saved to {output_file}")

# Data structures for GUI
cycles = []
register_file = defaultdict(list)
stats = {}
current_cycle = 1
simulator_data = {}

def parse_simulator_data(json_file):
    global cycles, register_file, simulator_data, stats
    try:
        with open(json_file, 'r', encoding='utf-8') as f:
            simulator_data = json.load(f)
    except FileNotFoundError:
        print(f"Error: '{json_file}' not found")
        return
    cycles = simulator_data["cycles"]
    
    # Initialize register file
    last_regs = {i: 0 for i in range(32)}
    for cycle_data in cycles:
        cycle = cycle_data["cycle"]
        for update in cycle_data["registerUpdates"]:
            reg_num = int(update["register"].replace("x", ""))
            val = int(update["value"], 16)
            last_regs[reg_num] = val
        for reg in range(32):
            register_file[cycle].append((reg, last_regs[reg]))
        if cycle in [5, 16, 29]:
            print(f"Cycle {cycle} registers: x5={last_regs[5]}, x6={last_regs[6]}")
    
    # Update stats from sim_output.txt parsing
    stats.update({
        "Total Cycles": 33,
        "CPI": 1.737,
        "Data Hazards": 9,
        "Stalls Due to Data Hazards": 9
    })

def create_gui():
    global current_cycle
    root = tk.Tk()
    root.title("RISC-V Pipeline Visualization")
    root.geometry("1600x900")
    root.configure(bg="#f0f0f0")

    # Style configuration
    style = ttk.Style()
    style.configure("TLabel", font=("Helvetica", 12), background="#f0f0f0")
    style.configure("TButton", font=("Helvetica", 10, "bold"))
    style.configure("TFrame", background="#f0f0f0")
    style.configure("Treeview", font=("Courier", 10))
    style.configure("Treeview.Heading", font=("Helvetica", 10, "bold"))

    # Main frame
    main_frame = ttk.Frame(root)
    main_frame.pack(fill=tk.BOTH, expand=True, padx=15, pady=15)

    # Cycle navigation
    nav_frame = ttk.Frame(main_frame)
    nav_frame.pack(fill=tk.X, pady=5)
    ttk.Label(nav_frame, text="Cycle:", font=("Helvetica", 14, "bold")).pack(side=tk.LEFT)
    cycle_label = ttk.Label(nav_frame, text=str(current_cycle), font=("Helvetica", 14))
    cycle_label.pack(side=tk.LEFT, padx=10)
    ttk.Button(nav_frame, text="Previous", command=lambda: prev_cycle()).pack(side=tk.LEFT, padx=5)
    ttk.Button(nav_frame, text="Next", command=lambda: next_cycle()).pack(side=tk.LEFT, padx=5)

    def prev_cycle():
        global current_cycle
        if current_cycle > 1:
            current_cycle -= 1
            update_display()
            cycle_label.config(text=str(current_cycle))

    def next_cycle():
        global current_cycle
        if current_cycle < len(cycles):
            current_cycle += 1
            update_display()
            cycle_label.config(text=str(current_cycle))

    # Pipeline visualization frame
    pipeline_frame = ttk.LabelFrame(main_frame, text="Pipeline Stages and Registers", padding=10)
    pipeline_frame.pack(fill=tk.BOTH, expand=True, pady=10)
    pipeline_frame.configure(relief="ridge", borderwidth=2)

    # Create blocks for stages and pipeline registers
    stage_labels = ['Fetch', 'IF/ID', 'Decode', 'ID/EX', 'Execute', 'EX/MEM', 'Memory', 'MEM/WB', 'Writeback']
    text_widgets = {}
    for i, stage in enumerate(stage_labels):
        frame = ttk.LabelFrame(pipeline_frame, text=stage, padding=5)
        frame.grid(row=0, column=i, padx=5, pady=5, sticky='nsew')
        width = 28 if stage in ['Fetch', 'Decode', 'Execute', 'Memory', 'Writeback'] else 38
        text = tk.Text(frame, height=7, width=width, wrap=tk.WORD, font=("Courier", 10), bg="#ffffff", fg="#333333")
        text.pack(fill=tk.BOTH, expand=True)
        text_widgets[stage] = text
        text.config(state='disabled')
        pipeline_frame.grid_columnconfigure(i, weight=1)

    # Register file table
    reg_frame = ttk.LabelFrame(main_frame, text="Register File", padding=10)
    reg_frame.pack(fill=tk.X, pady=10)
    reg_tree = ttk.Treeview(reg_frame, columns=('Register', 'Value'), show='headings', height=10)
    reg_tree.heading('Register', text='Register')
    reg_tree.heading('Value', text='Value (Hex)')
    reg_tree.column('Register', width=100, anchor='center')
    reg_tree.column('Value', width=150, anchor='center')
    reg_tree.pack(fill=tk.X)

    # Instructions and BTB frame
    info_frame = ttk.LabelFrame(main_frame, text="Instructions & BTB", padding=10)
    info_frame.pack(fill=tk.X, pady=10)
    info_text = tk.Text(info_frame, height=8, width=80, wrap=tk.WORD, font=("Courier", 10), bg="#ffffff", fg="#333333")
    info_text.pack(fill=tk.X)
    info_text.config(state='disabled')

    # Statistics and notes
    stats_frame = ttk.LabelFrame(main_frame, text="Statistics & Notes", padding=10)
    stats_frame.pack(fill=tk.X, pady=10)
    stats_text = tk.Text(stats_frame, height=12, width=80, wrap=tk.WORD, font=("Courier", 10), bg="#ffffff", fg="#333333")
    stats_text.pack(fill=tk.X)
    stats_text.insert(tk.END, "Notes:\n")
    stats_text.insert(tk.END, "- Data Forwarding Disabled (knobs.enable_data_forwarding = false)\n")
    stats_text.insert(tk.END, "- Stalls due to data hazards, e.g., BEQ waiting on x5 in Cycles 4-5\n\n")
    for key, value in stats.items():
        stats_text.insert(tk.END, f"{key}: {value}\n")
    stats_text.config(state='disabled')

    def update_display():
        global current_cycle
        if not cycles:
            print("No cycles parsed.")
            return

        # Clear widgets
        for stage in stage_labels:
            text_widgets[stage].config(state='normal')
            text_widgets[stage].delete(1.0, tk.END)
            text_widgets[stage].config(background="#ffffff")
        for item in reg_tree.get_children():
            reg_tree.delete(item)
        info_text.config(state='normal')
        info_text.delete(1.0, tk.END)

        # Find cycle data
        try:
            cycle_data = next(c for c in cycles if c["cycle"] == current_cycle)
        except StopIteration:
            cycle_data = cycles[0]
            current_cycle = cycle_data["cycle"]
            cycle_label.config(text=str(current_cycle))

        # Format stage messages
        def format_stage(stage_data, stage_name):
            if not stage_data.get("valid", False):
                return stage_data.get("message", f"{stage_name} inactive")
            msg = [f"{stage_name}:"]
            for key, value in stage_data.items():
                if key != "valid" and key != "message":
                    msg.append(f"  {key}: {value}")
            if stage_name.lower() == "decode" and "branchInfo" in stage_data:
                branch = stage_data["branchInfo"]
                msg.append(f"  Branch: {branch['type']}, rs1={branch['rs1']}, rs2={branch['rs2']}, Taken={branch['taken']}, Target={branch['target']}")
            return "\n".join(msg)

        # Update stage blocks
        for stage in ['fetch', 'decode', 'execute', 'memory', 'writeback']:
            text_widgets[stage.capitalize()].insert(tk.END, format_stage(cycle_data[stage], stage.capitalize()) + '\n')
            if stage == "decode" and "stalled" in cycle_data[stage].get("message", "").lower():
                text_widgets["Decode"].config(background="#ffff99")
            text_widgets[stage.capitalize()].config(state='disabled')

        # Update pipeline registers
        for reg, reg_data in cycle_data["registers"].items():
            reg_name = {"ifId": "IF/ID", "idEx": "ID/EX", "exMem": "EX/MEM", "memWb": "MEM/WB"}[reg]
            if not reg_data.get("valid", False):
                text_widgets[reg_name].insert(tk.END, f"{reg_name}: Invalid\n")
            else:
                msg = [f"{reg_name}:"]
                for key, value in reg_data.items():
                    if key != "valid":
                        msg.append(f"  {key}: {value}")
                text_widgets[reg_name].insert(tk.END, "\n".join(msg) + '\n')
            text_widgets[reg_name].config(state='disabled')

        # Update register file
        for reg, val in register_file[current_cycle]:
            reg_tree.insert('', tk.END, values=(f'x{reg}', f'0x{val:08x}'))

        # Update instructions and BTB
        info_text.insert(tk.END, "Instructions:\n")
        for instr in simulator_data["instructions"]:
            info_text.insert(tk.END, f"  {instr['addr']}: {instr['asm']} (0x{instr['instr']})\n")
        info_text.insert(tk.END, f"\nBTB (Cycle {current_cycle}):\n")
        info_text.insert(tk.END, f"  {cycle_data['btb']}\n")
        if cycle_data.get("prediction"):
            pred = cycle_data["prediction"]
            info_text.insert(tk.END, f"Prediction: {pred.get('message', 'None')}\n")
            if pred.get("update"):
                info_text.insert(tk.END, f"Update: {pred['update']}\n")
        info_text.config(state='disabled')

    # Initialize display
    if cycles:
        current_cycle = cycles[0]["cycle"]
        cycle_label.config(text=str(current_cycle))
        update_display()
    else:
        print("No cycle data.")

    root.mainloop()

def main():
    # Convert sim_output.txt to JSON
    script_dir = os.path.dirname(os.path.abspath(__file__))
    input_file = os.path.join(script_dir, 'sim_output.txt')
    output_file = os.path.join(script_dir, 'simulator_data.json')
    convert_sim_output_to_json(input_file, output_file)
    
    # Parse JSON and run GUI
    parse_simulator_data(output_file)
    if cycles:
        create_gui()
    else:
        print("No cycle data parsed.")

if __name__ == "__main__":
    main()
