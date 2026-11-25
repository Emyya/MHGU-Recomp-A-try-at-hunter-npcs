set -e
CF="--target=armv7a-none-eabi -march=armv8-a+crc -mfloat-abi=softfp -mfpu=crypto-neon-fp-armv8 -marm -fPIC -ffreestanding -fno-builtin -nostdlib -O2 -Wall -ffunction-sections -fdata-sections -fvisibility=default -mtp=soft"
BUILD=${NPC_BUILD:-$(head -c4 /dev/urandom | od -An -tx1 | tr -d " \n")}
echo "build $BUILD"
clang $CF -DNPC_BUILD="\"$BUILD\"" -c npc.c -o npc.o
clang --target=armv7a-none-eabi -march=armv8-a -c crt0.s -o crt0.o
ld.lld -T switch32.ld -shared --gc-sections --build-id=sha1 --version-script=exported.txt -init=init -fini=fini --export-dynamic -z max-page-size=0x1000 crt0.o npc.o -o npc.elf
python3 elf2nro.py npc.elf npc_hunter.nro
