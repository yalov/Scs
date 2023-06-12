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

#include "ScsInternal.h"

using namespace Scs;

Client::Client(const ClientParams & params) :
	m_port(params.port),
	m_address(params.address),
	m_timeoutMs(static_cast<long long>(params.timeoutSeconds * 1000.0)),
	m_updateMs(params.updateMs)
{
}

Client::~Client()
{
	m_status = Status::Shutdown;
	if (m_thread.joinable())
		m_thread.join();
}

void Client::Connect()
{
	m_status = Status::Shutdown;
	if (m_thread.joinable())
		m_thread.join();
	m_status = Status::Initial;
	m_error = false;;
	m_thread = std::thread([this]() { this->Run(); });
}

void Client::Run()
{
	// Create address from port and address strings
	AddressPtr address = CreateAddress(m_port, m_address);

	// Create a receive buffer
	BufferPtr receiveBuffer = CreateBuffer();
	receiveBuffer->reserve(RECEIVE_BUFFER_SIZE);

	// Create a receive queue
	ReceiveQueue receiveQueue;

	// Mark initial time
	auto statusTime = std::chrono::steady_clock::now();

	// Loop until we get a shutdown request
	while (m_status != Status::Shutdown)
	{
		// We don't want to burn too much CPU while idling.
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		if (m_status == Status::Initial)
		{
			m_socket = CreateSocket(address);
			m_socket->SetNonBlocking(true);
			if (!m_socket->Connect())
			{
				LogWriteLine("Error connecting client socket.");
				m_status = Status::Shutdown;
				m_error = true;
				break;
			}
			m_status = Status::Connecting;

			// Get current time
			m_lastUpdate = std::chrono::steady_clock::now();

			statusTime = m_lastUpdate;

            // On macOS, without this slight delay after a connect attempt, the socket returns immediate
            // success on IsWriteable(), even if no connection is present.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		else if (m_status == Status::Connecting)
		{
			if (std::chrono::steady_clock::now() > statusTime + std::chrono::milliseconds(m_timeoutMs))
			{
				if (address->Next())
				{
					m_status = Status::Initial;
					statusTime = std::chrono::steady_clock::now();
					LogWriteLine("Client failed to connect - trying next address.");
				}
				else
				{
					m_status = Status::ConnectionTimeout;
					LogWriteLine("Client failed to connect.");
				}
			}
			else
			{
				if (m_socket->IsWritable())
				{
                    m_status = Status::Ready;
                    LogWriteLine("Client established connection with server.");
					if (m_onConnect)
						m_onConnect(*this);
				}
			}
		}
		else if (m_status == Status::ConnectionTimeout)
		{
			// TODO: Wait some period and try again for a limited number of tries.  For now, we just fail
			LogWriteLine("Client retry not yet implemented.  Shutting down client.");
			m_error = true;
			m_status = Status::Shutdown;
		}
		else if (m_status == Status::Ready)
		{
			if (m_onUpdate)
			{
				auto nowTime = std::chrono::steady_clock::now();
				if ((nowTime - std::chrono::milliseconds(m_updateMs)) > m_lastUpdate)
				{
					m_onUpdate(*this);
					m_lastUpdate = nowTime;
				}
			}

			// Check first to see if we can write to the socket
			if (m_socket->IsWritable())
			{
				if (!m_sendQueue.Empty())
				{
					if (!m_sendQueue.Send(m_socket))
					{
						LogWriteLine("Error sending data to server.  Shutting down connection.");
						m_status = Status::Shutdown;
						break;
					}

					// We need to throttle our connection transmission rate
					std::this_thread::sleep_for(std::chrono::milliseconds(SEND_THROTTLE_MS));
				}
			}

			// Check for incoming data
			if (m_socket->IsReadable())
			{
				// Read data from socket
				receiveBuffer->resize(receiveBuffer->capacity());
				size_t bytesReceived = m_socket->Receive(receiveBuffer->data(), receiveBuffer->capacity(), 0);
				if (bytesReceived)
				{
					// Push data into the receive queue
					receiveBuffer->resize(bytesReceived);
					receiveQueue.Push(receiveBuffer->data(), receiveBuffer->size());
					receiveBuffer->clear();

					// Process data in receive queue
					BufferPtr receivedData = receiveQueue.Pop();
					while (receivedData)
					{
						if (m_onReceiveData)
							m_onReceiveData(*this, receivedData->data(), receivedData->size());
						receivedData = receiveQueue.Pop();
					}
				}
			}
		}
	}

	if (m_onDisconnect)
		m_onDisconnect(*this);
	m_socket = nullptr;
}

void Client::Send(const void * data, size_t bytes)
{
	m_sendQueue.Push(data, bytes);
}

ClientPtr Scs::CreateClient(const ClientParams & params)
{
	return std::allocate_shared<Client>(Allocator<Client>(), params);
}
