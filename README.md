# OP25-DVM
A simple patch program to patch IMBE from OP25 to DVM Project FNE

## This is presently VERY WIP. You will need a modified version of OP25 to export the IMBE frames to the FNE.
### The modified files will be available on here soon

# Usage Instructions

1. Clone the modified OP25 located at https://github.com/wv8vfd/op25
2. Install and configure OP25 in the standard ways. Check the boatbod github, RR fourms, or Youtube for help on that.
3. Clone this repo into your home directory or wherever you want it
4. Make a "build" directory in the folder that is cloned
5. Enter the build directory
6. Run `cmake ..`
7. Run `make`
8. Copy config.yml to the build directory
9. Edit config.yml for your system
10. Run OP25
11. Run `./op25-gateway`
12. You should start having any audio that would go across OP25 come across your selected talkgroup. 
