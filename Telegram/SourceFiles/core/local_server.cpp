#include "local_server.h"
#include "base/debug_log.h"
#include "core/core_settings.h"
#include "core/core_settings_proxy.h"
#include "core/application.h"
#include "storage/serialize_common.h"
#include "storage/localstorage.h"
#include <regex>

namespace Core {

LocalServer::LocalServer() : _localServerName("telegram_local_server")
{
	DEBUG_LOG(("LocalServer::LocalServer()"));
}

LocalServer::~LocalServer()
{
	DEBUG_LOG(("LocalServer::~LocalServer()"));
	_localServer.close();
}

void LocalServer::start()
{
	DEBUG_LOG(("LocalServer::start()"));
	QObject::connect(&_localServer, &QLocalServer::newConnection, this, &LocalServer::newInstaceConnected);
	if (!_localServer.listen(_localServerName)) {
		DEBUG_LOG(("LocalServer::start() failed"));
		return;
	}
}

void LocalServer::notifyNewServiceMessage(const QString& message)
{
	DEBUG_LOG(("LocalServer::notifyNewServiceMessage()"));
	for (auto &client : _localClients) {
		JsonRPCNotification notification;
		notification.method = "new_service_message";
		QJsonObject params;
		params["message"] = message;
		notification.params = params;
		client.first->write(notification.toJson());
		client.first->flush();
	}
}

void LocalServer::newInstaceConnected()
{
	DEBUG_LOG(("LocalServer::newInstaceConnected()"));
	for (auto client = _localServer.nextPendingConnection(); client; client = _localServer.nextPendingConnection()) {
		_localClients.push_back(LocalClient(client, QByteArray()));

		connect(client, &QLocalSocket::disconnected, this, &LocalServer::removeClients);
		connect(client, &QLocalSocket::readyRead, [this, client] {
			auto data = client->readAll();
			JsonRpcRequest request = JsonRpcRequest::fromStdString(data.toStdString());
			if (request.method == "set_proxy") {
				auto params = request.params.toObject();
				if (params.isEmpty()) {
					return;
				}
				MTP::ProxyData data;
				data.type		= params.value("protocol") == "socks5" ? MTP::ProxyData::Type::Socks5 : MTP::ProxyData::Type::Http;
				data.host		= params.value("host").toString();
				data.port		= params.value("port").toInt();
				data.user		= params.value("user").toString();
				data.password	= params.value("password").toString();
				setApplicationProxy(data);
			}
			// todo
		});
	}
}

void LocalServer::removeClients()
{
	DEBUG_LOG(("LocalServer::removeClients() Info: remove clients slot called, clients %1"
		).arg(_localClients.size()));
	if (_localClients.empty()) {
		return;
	}
	for (auto i = _localClients.begin(), e = _localClients.end(); i != e;) {
		if (i->first->state() != QLocalSocket::ConnectedState) {
			DEBUG_LOG(("LocalServer Info: removing client"));
			i = _localClients.erase(i);
			e = _localClients.end();
		}
		else {
			++i;
		}
	}
}

[[nodiscard]] QByteArray SerializeProxyData(const MTP::ProxyData& proxy) {
	auto result = QByteArray();
	const auto size = 1 * sizeof(qint32)
		+ Serialize::stringSize(proxy.host)
		+ 1 * sizeof(qint32)
		+ Serialize::stringSize(proxy.user)
		+ Serialize::stringSize(proxy.password);

	result.reserve(size);
	{
		const auto proxyType = [&] {
			switch (proxy.type) {
			case MTP::ProxyData::Type::None: return 0;
			case MTP::ProxyData::Type::Socks5: return 1;
			case MTP::ProxyData::Type::Http: return 2;
			case MTP::ProxyData::Type::Mtproto: return 3;
			}
			Unexpected("Bad type in SerializeProxyData");
		}();

		QDataStream stream(&result, QIODevice::WriteOnly);
		stream.setVersion(QDataStream::Qt_5_1);
		stream
			<< qint32(proxyType)
			<< proxy.host
			<< qint32(proxy.port)
			<< proxy.user
			<< proxy.password;
	}
	return result;
}

void LocalServer::setApplicationProxy(const MTP::ProxyData& data)
{
	DEBUG_LOG(("LocalServer::setApplicationProxy()"));
	Core::App().settings().proxy().list().push_back(data);
	Core::App().settings().proxy().setSelected(data);
	Core::App().settings().proxy().setSettings(MTP::ProxyData::Settings::Enabled);
	Core::App().settings().proxy().setUseProxyForCalls(true);
	Core::App().setCurrentProxy(data, MTP::ProxyData::Settings::Enabled);
	Local::writeSettings();
}

} // namespace Core