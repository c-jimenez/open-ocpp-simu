import argparse
import paho.mqtt.client as mqtt
import json
import time

def pass_badge(client: mqtt.Client, simu_name: str, badge: str):
    badge_msg = {"id": badge}
    client.publish(f"cp_simu/cps/{simu_name}/connectors/1/id_tag", json.dumps(badge_msg))

def set_cable(client: mqtt.Client, simu_name: str, cable: float, l1: float, l2: float, l3: float):
    cable_msg = {"cable": cable, "ready": True, "consumption_l1": l1, "consumption_l2": l2, "consumption_l3": l3}
    client.publish(f"cp_simu/cps/{simu_name}/connectors/1/car", json.dumps(cable_msg))

def start_charge(args, simu_name):
    print(f"starting charge on simu {simu_name}")
    pass_badge(client, simu_name, args.badge)
    time.sleep(0.3)
    set_cable(client, simu_name, args.cable, args.l1, args.l2, args.l3)

def stop_charge(args, simu_name):
    print(f"stop charge on simu {args.prefix}{index}")
    pass_badge(client, simu_name, args.badge)
    time.sleep(0.3)
    set_cable(client, simu_name, 0.0, 0.0, 0.0, 0.0)

def generate(args):
    print(f"generate station setup {args.file}")
    json_dict={}
    json_dict["charge_points"]=[]

    for index in range(args.sequence[0], args.sequence[1]+1):
        simu_name = f"{args.prefix}{index}"
        json_dict["charge_points"].append(
            {
                "id": simu_name,
                "vendor": "OpenOCPP",
                "model": "OpenOCPP",
                "serial": "CP_1",
                "central_system": f"ws://{args.broker}:{args.port}/",
                "nb_connectors": 1,
                "nb_phases": 1,
                "max_setpoint": 32,
                "max_setpoint_per_connector": 32,
                "voltage": 230.0,
                "type": "AC"
            })
        with open(args.file, "w") as write_file:
            json.dump(json_dict, write_file, indent=4)

parser = argparse.ArgumentParser(description='run commands on multiple simulators')
parser.add_argument('-b', '--broker', type=str, help="ip of the mqtt broker", default="127.0.0.1")
parser.add_argument('--broker_port', type=int, help="port of the mqtt broker", default=1883)
parser.add_argument('-p', '--prefix', required=True, type=str, help="prefix of simulator name")
parser.add_argument('-s', '--sequence', required=True, nargs=2, type=int,
                    help='minimal and maximum simulator name index')
command_parser = parser.add_subparsers(help='sub-parser for command to run')

parser_generate = command_parser.add_parser('generate', help="Generate json file for simulator creations")
parser_generate.set_defaults(func=generate, command="generate")
parser_generate.add_argument("--port", type=int, default=9980, help="OCPP port")
parser_generate.add_argument("--file", type=str, default="stations_setup.json", help="file output")

parser_start = command_parser.add_parser('start', help="start charge on stations")
parser_start.set_defaults(func=start_charge, command="start")
parser_start.add_argument("--badge", type=str, default="1234")
parser_start.add_argument("--cable", type=float, default=32.0)
parser_start.add_argument("--l1", type=float, default=32.0)
parser_start.add_argument("--l2", type=float, default=32.0)
parser_start.add_argument("--l3", type=float, default=32.0)

parser_stop = command_parser.add_parser('stop', help="stop charge on stations")
parser_stop.set_defaults(func=stop_charge, command="stop")
parser_stop.add_argument("--badge", type=str, default="1234")


# parse main arguments
args = parser.parse_args()
if not args.prefix:
    print("invalid simu prefix")
    exit(1)
if len(args.sequence) != 2 or (args.sequence[0] > args.sequence[1]):
    print("invalid sequence")
    exit(1)
print(args)

# Mosquitto broker
evce_broker = args.broker
evce_broker_port = args.broker_port
client = mqtt.Client("evce")
print(f"Connect to mqtt {evce_broker} {evce_broker_port}")
client.connect(evce_broker, evce_broker_port)

if (args.command == "generate"):
    args.func(args)
else:
    # Run command
    for index in range(args.sequence[0], args.sequence[1]+1):
        simu_name = f"{args.prefix}{index}"
        args.func(args, simu_name)
