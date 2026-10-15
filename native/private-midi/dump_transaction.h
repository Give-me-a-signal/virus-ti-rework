#pragma once
#include <chrono>
#include <stdexcept>
namespace virus::hardware {
// One caller owns the request mutex for this entire transaction. A timeout
// drains late fragments before retrying; writes are never retried here.
template<class Send,class Scan,class Pause,class Now>
auto readDump(Send send,Scan scan,Pause pause,Now now){
 for(unsigned attempt=0;attempt<3;++attempt){
  send();const auto deadline=now()+std::chrono::seconds(3);
  do{auto result=scan();if(!result.empty())return result;pause(10);}while(now()<deadline);
  if(attempt<2)pause(1500);
 }
 throw std::runtime_error("Edit-buffer request timeout after drained retries");
}
}
