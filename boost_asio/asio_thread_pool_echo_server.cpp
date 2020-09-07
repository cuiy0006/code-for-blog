#include "asio_thread_pool.hpp"

#include <boost/asio.hpp>
#include <array>
#include <iostream>
#include <thread>

using boost::asio::ip::tcp;

class TCPConnection : public std::enable_shared_from_this<TCPConnection> 
{
public:
    TCPConnection(boost::asio::io_service &io_service)
        : socket_(io_service),
          strand_(io_service)
    {        
    }
    
    tcp::socket &socket()
    {
        return socket_;
    }

    void start()
    {
        doRead();
    }
    
private:
    void doRead()
    {
        auto self = shared_from_this();


        boost::asio::async_read_until(socket_, sb, '\n',
                                      strand_.wrap([this, self](boost::system::error_code ec,
                                                        std::size_t bytes_transferred)
                                            {

                                                std::istream is(&sb);
                                                std::getline(is, buffer_);

                                                buffer_[buffer_.size() - 1]= '\n';

                                                std::cout << "read callback thread id: " << std::this_thread::get_id() << " ---- " << buffer_ << std::endl;
                                                if (!ec)
                                                {
                                                    std::this_thread::sleep_for(std::chrono::seconds(5));
                                                    doWrite();
                                                }
                                            }));

    }

    void doWrite()
    {
        auto self = shared_from_this();
        boost::asio::async_write(socket_, boost::asio::buffer(buffer_, buffer_.size()),
                                 strand_.wrap([this, self](boost::system::error_code ec,
                                                           std::size_t bytes_transferred)
                                              {
                                                  std::cout << "write callback thread id: " << std::this_thread::get_id() << "-----" << bytes_transferred << std::endl;
                                                  if (!ec)
                                                  {
                                                      doRead();                                         
                                                  }
                                              }));
    }

private:
    tcp::socket socket_;
    boost::asio::io_service::strand strand_;
    std::string buffer_;
    boost::asio::streambuf sb;
};

class EchoServer
{
public:
    EchoServer(boost::asio::io_service &main_io_service, boost::asio::io_service &sub_io_service, unsigned short port)
        : sub_io_service_ptr_(&sub_io_service),
          acceptor_(main_io_service, tcp::endpoint(tcp::v4(), port))
    {
        doAccept();
    }

    void doAccept()
    {
        auto conn = std::make_shared<TCPConnection>(*sub_io_service_ptr_);
        acceptor_.async_accept(conn->socket(),
                               [this, conn](boost::system::error_code ec)
                               {
                                   std::cout << "acceptor callback thread id: " << std::this_thread::get_id() << std::endl;
                                   if (!ec)
                                   {
                                       conn->start();
                                   }                                   
                                   this->doAccept();
                               });    
    }
    
private: 
    std::unique_ptr<boost::asio::io_service> sub_io_service_ptr_;
    tcp::acceptor acceptor_;
};

int main(int argc, char *argv[])
{
    //AsioThreadPool main_pool(1);
    boost::asio::io_service main_service;
    AsioThreadPool sub_pool(4);

    unsigned short port = 5800;
    EchoServer server(main_service, sub_pool.getIOService(), port);

    main_service.run();
    sub_pool.stop();
    
    return 0;
}
