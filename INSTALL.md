# Installing SD Bench on the Switch 2

You need a Nintendo Switch 2 running kaiHEN, with the homebrew menu, and a PC to put the files on the SD card.

1. Download `sdbench-nx2-1.0.0.zip` from the Releases page.
2. Put the console's SD card in your PC, or copy the file to the console with your usual file transfer.
3. Unzip the file onto the **root** of the SD card. If asked, allow it to merge with the existing `switch` folder. You should end up with:

```
SD card
└── switch
    └── sdbench.nro
```

4. Start **SD Bench** from the homebrew menu.

The test runs on start. It writes its temporary files and `results.txt` into `switch/sdbench/` and needs about 512 MB of free space.

To compare two cards, run it once with each card in the console and open the `results.txt` that is on each card. Every run adds a block to that file with the card's total and free size.
