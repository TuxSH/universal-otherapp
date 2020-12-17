# universal-otherapp

Otherapp payload compatible with system versions 1.0 to 11.14 (all regions, all models) that leverages full exploit chains to ultimately execute a payload from the SD card.

## Usage

This depends on the exploit. The recommended exploit for system versions 1.0 to 11.14 is [soundhax](https://github.com/nedwill/soundhax), in which case you just have to put `otherapp.bin` onto the root of your SD card.

## Technical details

We leverage a kernel exploit to alter L1 translation tables entries that were never previously accessed, then run `kernelhaxcode_3ds` which does the rest of the job.

* **Below system version 9.3**: we use memchunkhax1
* **9.3 and above**: we exploit `sm` then leverage this to exploit `spi`. SPI sysmodule has access to `GPUPROT`, subsequently allowing us to GPU DMA over the kernel memory
    * **Full writeup coming around Christmas**
    * `spi` vulnerability has been documented on 3dbrew for years
    * `sm` vulnerability is an unreported 0day, however I have fixed the bug in Luma3DS's reimpl back in 2017. I believe this is fine to release it now, as the 3DS is EoL and people can use seedminer on latest system version anyway
    * `safehax` or `agbhax` used depending on version

## Testing with Luma3DS

Need to disable firmlaunch patches & build without custom `sm` sysmodule if using Luma3DS.

## Credits

* @zoogie: testing and debugging on exotic firmware versions
* @fincs: exploitation ideas, etc.
* @aliaspider: memchunkhax code
