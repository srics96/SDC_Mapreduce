#include <conservator/ConservatorFrameworkFactory.h>
#include <zookeeper/zookeeper.h>

#include <trace.h>
#include <glog/logging.h>
#include <memory>
#include <string>

#include "stdafx.h"


using namespace std;

class ZookeeperHelper{

public:
    ConservatorFrameworkFactory factory;
    unique_ptr<ConservatorFramework> framework;


    ZookeeperHelper(string host);
    ZookeeperHelper();
    void get_masters_ordered(
        vector<string> &out_masters, bool strip_trailing = true);

    void delete_children(string path);

    void delete_recursive(string path);

    void del(string path);

    void create_if_not_exists(string path,string data = string());

    void create(string path,string data = string());

    void set(string path, string data);
};


static int get_trailing_num(const string& s) {
  return stoi(s.substr(s.find('_') + 1));
}



ZookeeperHelper::ZookeeperHelper(){
}


ZookeeperHelper::ZookeeperHelper(string host){
    factory = ConservatorFrameworkFactory();
    framework = factory.newClient(host);
    framework->start();
    auto c = framework->getChildren()->forPath("/masters");


}

void ZookeeperHelper::get_masters_ordered(
   vector<string>& out_masters, bool strip_trailing) {
  Trace trace(__func__);
  auto children = framework->getChildren()->forPath("/masters");
  sort(children.begin(), children.end(),
            [](const string& a, const string& b) {
              return get_trailing_num(a) < get_trailing_num(b);
            });

  out_masters.clear();
  for (auto s : children) {
    string push_str;
    if (strip_trailing) {
      push_str = s.substr(0, s.find_last_of('_'));
    } else {
      push_str = s;
    }
    out_masters.push_back(push_str);
  }
  
}

void ZookeeperHelper::delete_children(string path) {
  Trace trace(__func__, path);
  if (path.back() == '/') {
    path.pop_back();
  }
  auto children = framework->getChildren()->forPath(path);
  for (auto s : children) {
    auto res = framework->deleteNode()->forPath(path + "/" + s);
    assert(res == ZOK);
  }
  
}

void ZookeeperHelper::delete_recursive(string path) {
  Trace trace(__func__, path);
  if (path.back() == '/') {
    path.pop_back();
  }
  auto res = framework->deleteNode()->deletingChildren()->forPath(path);
  assert(res == ZOK);
  
}

void ZookeeperHelper::del(string path) {
  Trace trace(__func__, path);
  if (path.back() == '/') {
    path.pop_back();
  }
  auto res = framework->deleteNode()->forPath(path);
  assert(res == ZOK);
  
}

void ZookeeperHelper::create_if_not_exists(string path,string data) {
  Trace trace(__func__, "path=" + path + ", data=" + data);
  auto data_ptr = data.empty() ? nullptr : data.c_str();
  if (path.back() == '/') {
    path.pop_back();
  }
  if (framework->checkExists()->forPath(path) != ZOK) {
    auto ret = framework->create()->forPath(path, data_ptr);
    assert(ret == ZOK);
  }
  
}

void ZookeeperHelper::create(string path,string data) {
  Trace trace(__func__, "path=" + path + ", data=" + data);
  auto data_ptr = data.empty() ? nullptr : data.c_str();
  if (path.back() == '/') {
    path.pop_back();
  }
  auto res = framework->create()->forPath(path, data_ptr);
  assert(res == ZOK);
  
}

void ZookeeperHelper::set(string path,string data) {
  Trace trace(__func__, "path=" + path + ", data=" + data);
  if (path.back() == '/') {
    path.pop_back();
  }
  auto ret = framework->setData()->forPath(path, data.c_str());
  assert(ret == ZOK);
  
}

