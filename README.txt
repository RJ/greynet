Greynet - a darknet/mesh network plugin for Playdar
===================================================
This plugin logs into XMPP (gTalk/jabber/etc) and detects which of your friends
are also running playdar+greynet, it then makes a direct connection to all of 
them to form a darknet/mesh network, external to XMPP.

Queries and resulting files are streamed over the mesh in a darknet fashion,
so you'll only ever reveal your IP address or presence to your immediate 
friends.

Darknet 101
-----------
If Alice is friends with Bob, and Bob is friends with Charlie, but Alice and 
Charlie don't know eachother, it's possible that when Alice searches by 
querying her friends, Bob (if he doesn't have a matching result) will relay 
the query to Charlie, who btw won't know if the query originated from Bob or
not. If Charlie has a match and replies to Bob, Bob will relay the result to
Alice, who still doesn't know Charlie exists. To Alice, it looks like Bob has
the matching file. If Alice requests to stream it, Bob requests to Charlie and
acts as a proxy, so Alice only ever knows about Bob.

XMPP Note
---------
You can be logged in multiple times - just use the same account as you use for
chat with this plugin. The plugin doesn't receive msgs, and shouldn't confuse
anyone.

How to build it
===============

Gloox - an XMPP library
-----------------------
See: http://camaya.net/glooxdownload
You need to build gloox 1.0 from source.
At the time of writing, the latest ver was 1.0-beta-7:

$ sudo apt-get install libgnutls-dev
$ CXXFLAGS=-fPIC ./configure --without-openssl --with-gnutls --with-zlib
$ make && sudo make install

NB: gloox-1.0-beta7 doesn't like connecting to google talk servers if compiled
with OpenSSL - use GnuTLS instead.

libf2f - p2p servent/router library
-----------------------------------
See: http://github.com/RJ/libf2f
Build like so:

$ git clone git://github.com/RJ/libf2f.git
$ mkdir -p libf2f/build ; cd libf2f/build
$ cmake ..
$ make && sudo make install

libportfwd - a wrapper around upnp control libraries
----------------------------------------------------
See: http://github.com/RJ/libportfwd
This allows the app to set up a port fwd automatically on your nat router

$ git clone git://github.com/RJ/libportfwd.git
$ mkdir -p libportfwd/build ; cd libportfwd/build
$ cmake ..
$ make && sudo make install


Building the greynet plugin
---------------------------
Assuming you have a working playdar source tree.

$ cd playdar/resolvers
$ git clone git://github.com/RJ/greynet.git
$ cd ../build/
$ cmake ..
$ make greynet

The greynet plugin should load when you restart playdar.

Configuration
-------------
Slap something like this in your playdar.conf, in the "plugins" object:

"greynet" : 
{ 
    "jabber" : 
    {
        "jid" : "yournick@googlemail.com/playdar",
        "password" : "yourpass",
        "server" : "talk.google.com",
        "port" : 5222
    },
    "port" : 60211,
    "connection" : "nat"
}

The "server" line is only needed when using google talk - typically the server is the same as the domain in the jid.

If you are directly connected to the internet, change "connection": "direct"
and add: "ip" : "1.2.3.4" where 1.2.3.4 is your external IP address.

You can change the port to anything you want, it is sent to peers over XMPP.

