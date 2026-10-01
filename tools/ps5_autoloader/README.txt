Put payload ELFs here (etaHEN.elf, kstuff.elf, ...).

An autoload.txt in this directory lists what to launch, one per line,
top to bottom:

    etaHEN.elf        # names resolve relative to this directory
    !1500             # '#' comments; '!<ms>' sleeps before the next line
    kstuff.elf
    @sync             # after a clean load, move this dir onto the PS5
                      # (writes to the drive from the PS5 - experimental)

Without autoload.txt the unified-autoloader starts its bundled Payload
Manager instead. Full docs: project README (English / 简体中文).

把 payload ELF 放进本目录；autoload.txt 每行一个、自上而下依次加载；
没有该文件时启动自带的 Payload Manager。详见项目 README。
