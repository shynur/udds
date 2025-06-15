#! /bin/python3

import sys
import argparse
import html

assert len(sys.argv) > 1 and (
    '-h' in sys.argv[1:] or '--help' in sys.argv[1:]
)

parser = argparse.ArgumentParser(
    description="""
同局域网或同主机內 DDS 广播服务.
指令由用户输入, 查询结果通过标准输出打印.

注意: 传递参数时必须用 '=' 赋值, 不允许使用空格分隔形参和实参.
""",
    allow_abbrev=False,
    formatter_class=argparse.RawTextHelpFormatter,
    epilog = html.unescape('&copy;') + " 2025  <https://github.com/shynur>.",
)

parser.add_argument(
    "--robot_id",
    required=True,
    type=str,
    metavar="ROBOT_ID",
    help="""小车的名字, 用于区分同一个广播域内不同的小车.
该名字不允许包含空白字符.
注意: 同一台小车若创建多个 uDDS 进程, 则传递给每个进程的小车名须不同.
""",
)

parser.add_argument(
    "--development_mode",
    action="store_true",
    help="打印日志用于 debug",
)

parser.add_argument(
    "--fastdds_domain",
    type=int,
    required=False,
    default=1,
    # choices=range(1, 2**32),
    metavar="DOMAIN_NUMBER",
    help="""正整数, 用于区分同一个局域网下不同的 DDS 广播域 (位于相同广播域才能相互发现).
注意: 目前只接受 1 作为参数.
""",
)

parser.add_argument(
    "--end_of_json",
    required=False,
    type=str,
    metavar="EOJ",
    default="shynur.udds.json.end",
)

args = parser.parse_args()

assert all(not c.isspace() for c in args.robot_id)
assert all(not c.isspace() for c in args.end_of_json)
assert 1 <= args.fastdds_domain < 2**32
