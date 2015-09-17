# autostarter
little c-program that reads ~/.config/autostart/*.desktop-files and starts the given executables

I use it to startup my X11-Session with awesome 
it is useful because ~/.config/autostart is a standart dir, and owncloud-client
and dropbox-client both create files in there already. Others will probably follow.

# Compile
just compile it with 
gcc autostart.c -o autostart

# Usage

./autostart 
Will just start all .desktop-entries in ~/.config/autostart
You can put this e.g. in .xinitrc 

./autostart ~/path/to/desktop/entries
Starts all entries in given path


# Notes
I developed this helper to be used with Window Managers that do not automatically execute these .desktop files, it is useless for WMs like gnome.

This executable forks and uses exec to start the autostart-programs as children. When it exists, all these created children will be parentless, which might mean that they won't be killed by your Windowmanager.
