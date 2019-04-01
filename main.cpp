#include <iostream>
#include <string>
#include <vector>
#include "oskidb.h"

int main() {
  oskidb::WriteOptions wo;
  oskidb::ReadOptions ro;
  std::vector<std::string> words = {"wow", "boomshakalakaboomshakalakaboomshakalakalaka", "kowloon", "pancake", "oyster", "latitude"};

  [&](){
    oskidb::DbWriter db("wow.db");
    for (auto &word : words)
      db.put(word, std::to_string(word.size()), wo);
  }();

  [&](){
    oskidb::DbReader db("wow.db");
    for (auto &word : words) {
      std::string result;
      oskidb::Status s = db.get(word, &result, ro);
      std::cout << "size of " << word << ": " << result << '\n';
    }
  }();
}