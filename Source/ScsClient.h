/*
The MIT License (MIT)

Copyright (c) 2018 James Boer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#pragma once
#ifndef SCS_CLIENT_H____
#define SCS_CLIENT_H____

namespace Scs
{

	class Client : public IClient
	{
	public:
		Client(const ClientParams & params);
		virtual ~Client() override;

		void Connect() override;
		bool IsConnected() const override { return m_status == Status::Ready ? true : false; }
		bool HasError() const override { return m_error; }

		void OnConnect(ClientOnConnectFn onConnect) override { assert(m_status == Status::Initial); m_onConnect = onConnect; }
		void OnDisconnect(ClientOnDisconnectFn onDisconnect) override { assert(m_status == Status::Initial); m_onDisconnect = onDisconnect; }
		void OnReceiveData(ClientOnReceiveDataFn onReceiveData) override { assert(m_status == Status::Initial); m_onReceiveData = onReceiveData; }
		void OnUpdate(ClientOnUpdateFn onUpdate) override { assert(m_status == Status::Initial); m_onUpdate = onUpdate; }

		void Send(const void * data, size_t bytes) override;

	private:
		void Run();

		enum class Status
		{
			Initial,
			Connecting,
			ConnectionTimeout,
			Ready,
			Shutdown,
		};

		SocketPtr m_socket;
		std::thread m_thread;
		ClientOnConnectFn m_onConnect;
		ClientOnDisconnectFn m_onDisconnect;
		ClientOnReceiveDataFn m_onReceiveData;
		ClientOnUpdateFn m_onUpdate;
		String m_port;
		String m_address;
		long long m_timeoutMs;
		uint32_t m_updateMs;
		std::chrono::time_point<std::chrono::steady_clock> m_lastUpdate;
		std::atomic<Status> m_status = Status::Initial;
		std::atomic_bool m_error = false;
		SendQueue m_sendQueue;
	};

} // namespace Scs

#endif // SCS_CLIENT_H____
