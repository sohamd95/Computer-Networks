## How to use -

### Compilation -

Both server and clients are dependent on [GNU Readline](https://tiswww.case.edu/php/chet/readline/rltop.html).

```
sudo apt-get install libreadline6-dev
```
Once you have readline, compile server and client using -
```
cd Chatroom/
make
```

Once compiled, you are ready to chat!

Start the server first -

```
./server.out
```
The server will run on port ```12345```. If you'd like to change that, you'll have to change it in the source code and recompile. (Not the best coding practice; I know)


Then, in a new terminal window,

```
./client
```

You can open as many clients as you like.
