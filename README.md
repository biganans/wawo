# wawo
wawo is a network lib providing multi-threading io support across windows&amp;posix platforms  

#features
  1, windows and posix platform compatible   
  2, about 300K QPS on i7 4770 (runing on os centos 7.1) with 50% cpu usage on ping <--> pong test case   
  3, friendly to define customized business message and network packet protocol  

#build requirement
  compiler: c++ 11  
  msvc ide : vc 2012 update 4  
  gcc：gcc 4.8x  　　   

#build proj
###windows
  libs/wawo/projects/msvc/wawo.2012.vcxproj      
  
###linux
  /libs/wawo/projects/linux/makefile    
      usage example     
      make build=debug platform=x86_32    
      make build=release platform=x86_64
  
  
###codeblock
  /libs/wawo/projects/codeblocks/wawo     


#example
  examples\     


#basic concept

###packet
  a pakcet is basically a chunk of network bytes sequence, it is usually the basic transfer unit in between two sockets.


###socket
  a socket is a packet transfer, we use socket to send packets to a remote socket, and receive packet from remote socket.    
  once a a socket received a packet from remote end, or a error occured on this socket, as a socket event dispatcher, socket will trigger a socket event , and its listener would hear this socket event immediately. the listener would be a peer, or a socket proxy.     
  
  socket is a template class in wawo's implementation as below   
  
  by this way, 
  you can easyly custom your own protocol implementation by inheriting Protocol_Abstract. and pass your own protocol class as template arguments to instance a new socket type.          


###io event and socket event  
  each event would be planed on task pool and executed on threading pools concurrently.     
  we have io event and socket event right now.    
  io event tells what happens on a given socket fd:     
         
        1, fd can read        
        2, fd can write       
        3, fd error   
        ...
      and so on       
      for a complete event list , please refer to wawo\net\core\NetEvnet.hpp     
  
  
  socket event tells what happens on a given socket object.         
  
        1, socket received a packet   
        2, socket closed     
        3, socket encounter a error
        ....
      and so on       
      for a complete event list , please refer to wawo\net\core\NetEvnet.hpp

  notice:    
  socket read, and socket write can execute concurrently on thread safe level .

###socket proxy
  socket proxy is a socket manager and its main responsibility is as follows:   
    a, hold a reference of sockets   
    b, socket error handing   
    c, work as a observer to do asyn connect to a remote server    

###socket observer
  socket observer is just as what its name is , its main responsibility is as follows:    
    a, register and unregister socket event for non-blocking sockets    
    b, watch sockets io event, and enqueue the relevant event into the task pool   



###peer
  peer is the basic client unit of socket based programme in wawo, and it is a listener a peer can holds several sockets, once a socket packet arrived , peer 

peer proxy

service provider



listener


dispatcher




#schedule
  1, implement protocol, message, and peer for the followwing network protocol  
      a, http  
      b, https  
      c, websocket  
      
  2, implement RPC component     
  3, integrate script support for lua and javascript   
  4, up to you  
  
