import csv
import os
import subprocess
import sys


COLUMNS = [
    "phase", "instructions", "cycles", "ipc", "accesses",
    "instruction_hits", "instruction_misses",
    "data_hits", "data_misses", "pte_hits", "pte_misses",
    "stlb_misses", "l1d_misses", "l1i_misses",
    "stlb_mpki", "l1d_mpki", "l1i_mpki",
]


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 scripts/save_l2_windows.py ./bin/base <simulation arguments>", file=sys.stderr)
        return 1

    script_folder = os.path.dirname(os.path.abspath(__file__))
    project_folder = os.path.dirname(script_folder)
    data_folder = os.path.join(project_folder, "data")

    # The build script sets the CSV filename.
    csv_name = os.environ.get("L2_CSV", "base_phases.csv")
    csv_name = os.path.basename(csv_name)
    csv_path = os.path.join(data_folder, csv_name)

    phase_number = 1
    phase_size = None
    roi_start = None
    simulator = None

    try:
        os.makedirs(data_folder, exist_ok=True)
        with open(csv_path, "w", newline="") as csv_file:
            writer = csv.writer(csv_file, lineterminator="\n")
            writer.writerow(COLUMNS)

            # Start ChampSim and read its output one line at a time.
            simulator = subprocess.Popen(
                sys.argv[1:], stdout=subprocess.PIPE, universal_newlines=True
            )

            for line in simulator.stdout:
                if line.startswith("PHASE_START,"):
                    if roi_start is not None:
                        raise ValueError("Duplicate phase start")
                    roi_start = int(line.strip().split(",")[1])
                    if roi_start < 0:
                        raise ValueError("Invalid phase start")
                    print(line, end="", flush=True)

                elif line.startswith("PHASE_WINDOW,"):
                    parts = line.strip().split(",")
                    values = []
                    for value in parts[1:]:
                        values.append(int(value))

                    if len(values) != 11 or min(values) < 0:
                        raise ValueError("Invalid phase counts")

                    (
                        end_instruction, cycles,
                        instruction_hits, instruction_misses,
                        data_hits, data_misses,
                        pte_hits, pte_misses,
                        stlb_misses, l1d_misses, l1i_misses,
                    ) = values

                    # The first window tells us the size set in phase.cc.
                    if phase_size is None:
                        phase_size = end_instruction

                    if (end_instruction != phase_number * phase_size
                            or phase_size <= 0 or cycles <= 0 or roi_start is None):
                        raise ValueError("Invalid phase boundaries or counts")

                    ipc = phase_size / cycles
                    accesses = (instruction_hits + instruction_misses
                                + data_hits + data_misses + pte_hits + pte_misses)

                    # Miss counts already belong to this window; C++ does the subtraction.
                    stlb_mpki = stlb_misses * 1000 / phase_size
                    l1d_mpki = l1d_misses * 1000 / phase_size
                    l1i_mpki = l1i_misses * 1000 / phase_size

                    writer.writerow([
                        phase_number, phase_size, cycles, ipc, accesses,
                        instruction_hits, instruction_misses,
                        data_hits, data_misses, pte_hits, pte_misses,
                        stlb_misses, l1d_misses, l1i_misses,
                        stlb_mpki, l1d_mpki, l1i_mpki,
                    ])
                    csv_file.flush()
                    phase_number += 1

                else:
                    print(line, end="", flush=True)

            exit_code = simulator.wait()
            if exit_code < 0:
                return 128 - exit_code
            return exit_code

    except (OSError, ValueError) as error:
        print("Error: " + str(error), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130
    finally:
        if simulator is not None and simulator.poll() is None:
            simulator.terminate()
            simulator.wait()


if __name__ == "__main__":
    sys.exit(main())
