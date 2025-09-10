# Velo2

Note taking brainstorming app

![velo2.jpg](https://gist.github.com/user-attachments/assets/b029a9b1-be2c-4c45-8f50-283d6210e506.jpg?raw=true)

# Features:

* create notes, paper notes
* connect elements with arrows
* organize elements into nested spaces
* paste images from clipboard and annotate them
* stora all data in a single portable SQLite3 database file
* resize, move, fork, clone (by text or by size) and delete elements
* infinite canvas space

## Dependencies

GTK4, sqlite3

```
sudo apt install libgtk-4-dev
sudo apt install libsqlite3-dev
```

## To run on x86_64 GNU/Linux:

`make -B -j 7 && ./velo2`
