# universal-otherapp

Otherapp payload compatible with system versions 1.0 to 11.15 (all regions, all models) that leverages full exploit chains to ultimately execute a payload from the SD card.

## Usage

This depends on the exploit. The recommended exploit for system versions 1.0 to 11.3 is [soundhax](https://github.com/nedwill/soundhax), in which case you just have to put `otherapp.bin` onto the root of your SD card.

## Technical details

We leverage a kernel exploit to alter L1 translation tables entries that were never previously accessed, then run `kernelhaxcode_3ds` which does the rest of the job.

* **Below system version 9.3**: we use memchunkhax1
* **9.3 to 11.15**: we exploit `sm` then leverage this to exploit `spi`. SPI sysmodule has access to `GPUPROT`, subsequently allowing us to GPU DMA over the kernel memory
    * `spi` vulnerability has been documented on 3dbrew for years, and hasn't yet been fixed
    * `sm` vulnerability is an unreported 0day. Fixed in system version 11.16
    * `safehax` or `agbhax` used depending on version. `agbhax` hasn't yet been fixed

## Testing with Luma3DS

Need to disable firmlaunch patches & build without custom `sm` sysmodule if using Luma3DS.

## Credits

* @zoogie: testing and debugging on exotic firmware versions
* @fincs: exploitation ideas, etc.
* @aliaspider: memchunkhax code
