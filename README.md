# universal-otherapp

Otherapp payload compatible with system versions 1.0 to 11.3 (all regions, all models) that leverages full exploit chains to ultimately execute a payload from the SD card.

## Usage

This depends on the exploit. The recommended exploit for system versions 2.1 to 11.13 is [soundhax](https://github.com/nedwill/soundhax), in which case you just have to put `otherapp.bin` onto the root of your SD card.

## Technical details

We leverage a kernel exploit to alter L1 translation tables entries that were never previously accessed, then run `kernelhaxcode_3ds` which does the rest of the job.

* **Below system version 9.3**: we use memchunkhax1
* **9.3 and above**: we use LazyPixie. As it more or less requires access to `svcReplyAndReceive`, we leverage a variant of `httpwn`:
    * `httpwn` was believed to be fixed on 11.4, however the heap code being used is still vulnerable at the time of writing (see [here](https://gist.github.com/TuxSH/854b2ba84bd7980be598c3d076dc1fcb))
    * `LazyPixie` was fixed on 11.12

## Restrictions

Cannot be directly used by applet (e.g. browser) exploits, need to takeover an application first.
Need to disable firmlaunch patches if using Luma3DS.

## Credits

* @fincs: LazyPixie exploitation ideas, etc.
* @aliaspider: memchunkhax code
