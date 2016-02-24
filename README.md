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
  windows libs/wawo/projects/msvc/wawo.2012.vcxproj  
  linux  /libs/wawo/projects/linux/makefile   
  codeblock /libs/wawo/projects/codeblocks/wawo  


#example
  examples\     


#basic concept

packet

message

socket

socket proxy

socket observer

socket event

peer

peer proxy


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
  
