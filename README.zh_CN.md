# wawo
wawo is a network message dispatche lib providing multi-threading io for windows&posix platforms  

#features
     
    1, windows and posix platform compatible   
    2, message dispatche driven by system io read/write event   
    3, about 300K QPS at peak and a average of 250K on i7 4770 (running on OS centos 7.1) with 50% cpu usage on ping <--> pong test case   
    4, friendly to define customized business message and network packet protocol  
  
  
#build requirement
   
    your compiler must support c++ 11     
    
    suggest tool     
    msvc ide: vc 2012 update 4     
    gcc: gcc 4.8x    　　   

#build projects
###windows
  
  libs/wawo/projects/msvc/wawo.2012.vcxproj      
  
###linux
  
  /libs/wawo/projects/linux/makefile    
      usage          
	  
       make build=debug platform=x86_32    
       make build=release platform=x86_32    
       make build=debug platform=x86_64    
       make build=release platform=x86_64   
      
  
###codeblock
  
     /libs/wawo/projects/codeblocks/wawo     


#example
  
    examples\     


#basic concept

###packet
  
    a pakcet is a chunk of network bytes sequence, it is a basic transfer unit in between two sockets    
  
  
###socket
  
    a socket is a packet transfer, we use it to send and packets to a remote socket, and receive packets from a remote socket.  
    once a socket receives a packet from remote end, or a error occurs, then the socket as a socket event dispatcher will trigger a socket event , and then its listener should hear this socket event immediately if the event have been registered before. the listener can be a peer, or a socket proxy.     
  
    socket is a template class in wawo's implementation.      
  
    by doing so    
    we can easily custom your own protocol implementation by inheriting wawo::net::core::Protocol_Abstract. and pass your own protocol class as template argument to instantiate a new socket type.          
  

###io event and socket event 
  
  each event is planed on task pool and is executed on threading pool concurrently.     
  currently, we have io event and socket event    
  io event tells what happens on a given socket fd:     
         
        1, fd can read        
        2, fd can write       
        3, fd error   
        ...
      and so on       
      for a complete event list , please refer to wawo\net\core\NetEvnet.hpp     
  
  
  socket event tells what happens on a given socket object:         
  
        1, socket received a packet   
        2, socket closed     
        3, socket encounter a error   
        ....
      and so on       
      for a complete event list , please refer to wawo\net\core\NetEvnet.hpp

  Note:    
  socket read&write can execute concurrently on thread safe level .

###socket proxy
  
  socket proxy is a socket manager and its main responsibility is as below:   
    a, hold a reference of sockets   
    b, socket error handing   
    c, work as a observer to do asyn connect to a remote server    

###socket observer
  
  socket observer is just as what its name suggests , its main responsibility is as below:    
    a, register and unregister socket event for non-blocking sockets    
    b, watch sockets io event, and enqueue the relevant event into the task pool   



###peer
  
    peer is the basic client unit of socket based programme in wawo.    
    a peer is a socket event listener.    
    a peer can hold several sockets.   
  
  
###peer proxy
    peer proxy works as peer manager and it is also a service holder


###service provider
  
  service provider is as what its name suggests, it service the message by messsage id, all the business message with a service id of this service would be dispatched to the provider.    
  
  if u want to build a new service provider, follow below steps:
          
          a, inherit from wawo::net::ServiceProvider_Abstract   
          b, implement virtual function     
                	virtual void HandleMessage( MyBasePeerMessageCtxT const& ctx, WAWO_SHARED_PTR<MyMessageT> const& incoming ) = 0;            
          c, register this service to your peer proxy by 
            	 peer_proxy_instance->Register(service_id, your_service_provider);    
            	 
            	 
  



###listener
    
    a listener can register a given type of event to the relevant dispatcher, and then get notified immediately if dispatcher trigger that event   
###dispatcher
    
    a dispatcher can trigger event to its listener
	a dispatcher can also raise event and plan this event to the task pool to be executed asynchronous on thread pool.  



#schedule

  
   
    1, implement RPC component     
    2, integrate script support for lua and javascript   
    3, implement support for distributed message dispatch infrastructure   
    4, implement protocol, message, and peer for the followwing network protocol  
          a, http  
          b, https  
          c, websocket  
      
    5, up to you  
  


#Discussion&Contact 
       QQ 群：452583496    
       mail: io.cpp@outlook.com    
  
  
