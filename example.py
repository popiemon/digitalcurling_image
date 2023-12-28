import subprocess
import threading
import json
from dataclasses import dataclass
from copy import deepcopy
from typing import Optional, Any
import time

global first_flag


@dataclass
class ShotParam:
    x: float
    y: float
    rotation: str
    game_state: Optional[dict[str, Any]]


def read_output(process):
    global first_flag
    while True:
        output = process.stdout.readline()
        if output.decode().strip() in ["won the game", "lost the game"]:
            quit()
        if output.decode().strip().startswith("inputwait"):
            first_flag = True
            game_state = json.loads(output.decode().strip().split()[1])
            shot_param = select_shot(deepcopy(game_state), process)
            shot(shot_param, False, process)


def shot(shot_param: ShotParam, preview: bool, process) -> Optional[dict[str, Any]]:
    global first_flag
    if preview is False:
        mode = "shot"
    elif shot_param.game_state:
        mode = "simu"
    else:
        mode = "simufile"
    if first_flag is True:
        cpp_input = f"{shot_param.x} {shot_param.y} {shot_param.rotation} {mode} {json.dumps(shot_param.game_state)}\n" #送信する入力データ
        process.stdin.write(cpp_input.encode())
        process.stdin.flush()
    else:
        while True:
            output = process.stdout.readline()
            if output.decode().startswith("inputwait"):
                cpp_input = f"{shot_param.x} {shot_param.y} {shot_param.rotation} {mode} {json.dumps(shot_param.game_state)}\n"
                process.stdin.write(cpp_input.encode())
                process.stdin.flush()
                break
    if mode != "shot":
        first_flag = False
        while True:
            output = process.stdout.readline()
            if output.decode().startswith("jsonoutput"):
                game_state = json.loads(output.decode().strip().split()[1])
                return game_state
            if output.decode().strip() in ["won the game", "lost the game"]:
                quit()
    else:
        return None


def simulate(shot_param: ShotParam, process) -> dict[str, Any]:
    game_state = shot(shot_param, True, process)
    return game_state


def select_shot(game_state: dict[str, Any], process) -> ShotParam:
    """
    simulate & shot
    """
    for _ in range(3):
        # 指定の盤面に対してsimulation
        shot_param = ShotParam(0.1, 2.5, "cw", game_state)
        simu_game_state = simulate(shot_param, process)
    # 実際に決めた石を投げる
    return ShotParam(0.1, 2.5, "ccw", None)


def main(port=10000):
    cpp_command = [f"/workspace/DigitalCurling3-ClientExamples/build/stdio/digitalcurling3_client_examples__stdio", "localhost", str(port)]

    cpp_process = subprocess.Popen(cpp_command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    output_thread = threading.Thread(target=read_output, args=(cpp_process,))
    output_thread.start()


if __name__ == "__main__":
    main()
