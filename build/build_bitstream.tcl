# ============================================================================
#  build/build_bitstream.tcl —— 从现有 Vivado 工程重建位流(M5「可复现构建脚本」)
# ============================================================================
#
#  用法(工程目录可用环境变量覆盖):
#
#      export PATH=$HOME/xilinx/2025.2/Vivado/bin:$PATH
#      QRS_PROJ_DIR=$HOME/Desktop/fpga_pro/qrs \
#          vivado -mode batch -notrace -source build/build_bitstream.tcl \
#          -log build/vivado_build.log -journal build/vivado_build.jou
#
#  产物:
#      $QRS_PROJ_DIR/qrs.runs/impl_1/system_wrapper.bit   ← 位流
#      $QRS_PROJ_DIR/qrs.runs/impl_1/system_wrapper.hwh   ← 硬件描述(BD 动了才会变)
#      build/summary.txt                                  ← 本脚本汇总的判据
#
#  ⛔ **本脚本不跑 BD**。只改 RTL 常数 / 逻辑时 `.hwh` 逐字节不变(2026-10-12 实测),
#     那是"BD 没动"的现成判据。真要动 BD 请另跑 `build/bd_add_*.tcl`。
#
#  ⚠️ **这是完整重建**(reset synth + impl),不是增量。
#     理由:增量最怕的是"改了源却没被吃掉"——`reset_run` 之后不存在这个状态。
#     代价是十几分钟,对一次上电周期只做一次的事,不值得为省这几分钟冒险。
#
#  ⚠️ **工程目录里不要留 `xsim.dir` / `xvlog.log`** —— 那是 `xvlog` 单文件语法检查
#     的副产物,和本工程无关,混在 `sources_1/new/` 里会被误当成源文件。
# ============================================================================

set jobs 8

if {[info exists ::env(QRS_PROJ_DIR)]} {
    set proj_dir $::env(QRS_PROJ_DIR)
} else {
    set proj_dir "$::env(HOME)/Desktop/fpga_pro/qrs"
}

set xpr "$proj_dir/qrs.xpr"
if {![file exists $xpr]} {
    puts "ERROR: 找不到工程 $xpr —— 用 QRS_PROJ_DIR 指定"
    exit 1
}

puts "== QRS build: proj_dir = $proj_dir"

open_project $xpr

# ---- 1. 综合 ----------------------------------------------------------------
# ⛔ 必须 reset:否则 Vivado 可能复用旧 dcp,RTL 改了却**静默**用旧网表综合。
reset_run synth_1
launch_runs synth_1 -jobs $jobs
wait_on_run synth_1

if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
    puts "ERROR: synth_1 未完成(progress = [get_property PROGRESS [get_runs synth_1]]),status = [get_property STATUS [get_runs synth_1]]"
    exit 1
}
puts "== synth_1 OK"

# ---- 2. 实现 + 位流 ---------------------------------------------------------
reset_run impl_1
launch_runs impl_1 -to_step write_bitstream -jobs $jobs
wait_on_run impl_1

if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
    puts "ERROR: impl_1 未完成(progress = [get_property PROGRESS [get_runs impl_1]]),status = [get_property STATUS [get_runs impl_1]]"
    exit 1
}
puts "== impl_1 OK"

# ---- 3. 汇总判据(M3「综合与实现报告」的正文素材)-------------------------
# 打开实现后的设计,读时序 / 资源 / DRC 写到 `summary.txt`。
# 🔴 **汇总逻辑放在 `build/report_summary.tcl`**,不是一个文件里抄一遍 ——
#    那个脚本能**单独跑**(不动综合结果,只想重取一次数),这里 `source` 它复用。
open_run impl_1
source [file join [file dirname [info script]] report_summary.tcl]

puts "== 位流:$proj_dir/qrs.runs/impl_1/system_wrapper.bit"
exit 0
