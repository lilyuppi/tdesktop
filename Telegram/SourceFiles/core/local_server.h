#pragma once

#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>
#include <QJsonObject>
#include <QJsonDocument>

namespace MTP {
struct ProxyData;
} // namespace MTP

namespace Core {

struct JsonRpc {
	const QString jsonrpc = "2.0";
	QString id;
	virtual ~JsonRpc() = default;
	virtual QByteArray toJson() const = 0;
};
struct JsonRPCNotification : public JsonRpc {
	QString method;
	QJsonValue params;
	JsonRPCNotification() = default;
	~JsonRPCNotification() = default;
	JsonRPCNotification(const QString& method, const QJsonValue& params) : method(method), params(params) {}

	QByteArray toJson() const {
		QJsonObject json;
		json["jsonrpc"] = jsonrpc;
		json["method"] = method;
		json["params"] = params;
		return QJsonDocument(json).toJson();
	}
};
struct JsonRpcRequest : public JsonRpc {
	QString method;
	QJsonValue params;

	JsonRpcRequest() = default;
	~JsonRpcRequest() = default;
	JsonRpcRequest(const QString& id, const QString& method, const QJsonValue& params) : method(method), params(params) {
		this->id = id;
	}
	
	QByteArray toJson() const {
		QJsonObject json;
		json["jsonrpc"] = jsonrpc;
		json["id"] = id;
		json["method"] = method;
		json["params"] = params;
		return QJsonDocument(json).toJson();
	}
	
	// Deserialize from JSON string
    static JsonRpcRequest fromStdString(const std::string& str) {
		QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(str));
		QJsonObject json = doc.object();
		QString id = json["id"].toString();
		QString method = json["method"].toString();
		QJsonValue params = json["params"];
		return JsonRpcRequest(id, method, params);
    }
};
struct JsonRpcResponse : public JsonRpc {
	QJsonValue result;
	QJsonValue error;

	JsonRpcResponse() = default;
	~JsonRpcResponse() = default;
	QByteArray toJson() const {
		QJsonObject json;
		json["jsonrpc"] = jsonrpc;
		json["id"] = id;
		if (!result.isNull()) {
			json["result"] = result;
		} else {
			json["error"] = error;
		}
		return QJsonDocument(json).toJson();
	}

	// Deserialize from JSON string
	static JsonRpcResponse fromStdString(const std::string& str) {
		QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(str));
		QJsonObject json = doc.object();
		JsonRpcResponse response;
		response.id = json["id"].toString();
		if (json.contains("result")) {
			response.result = json["result"];
		} else {
			response.error = json["error"];
		}
		return response;
	}
};

class LocalServer : public QObject
{
	Q_OBJECT
public:
	LocalServer();
	~LocalServer();
	void start();
	void notifyNewServiceMessage(const QString& message);

private:
	typedef QPair<QLocalSocket*, QByteArray> LocalClient;
	typedef QList<LocalClient> LocalClients;

	void newInstaceConnected();
	void removeClients();
	void setApplicationProxy(const MTP::ProxyData& data);

	QString _localServerName;
	QLocalServer _localServer;
	LocalClients _localClients;
};
} // namespace Core