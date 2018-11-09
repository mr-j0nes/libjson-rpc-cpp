#include "abstractprotocolhandler.h"
#include "../exception.h"
#include "../jsonparser.h"

#include <map>

using namespace jsonrpc;
using namespace std;

AbstractProtocolHandler::AbstractProtocolHandler(
    IProcedureInvokationHandler &handler)
    : handler(handler) {}

AbstractProtocolHandler::~AbstractProtocolHandler() {}

void AbstractProtocolHandler::AddProcedure(const Procedure &procedure) {
  this->procedures[procedure.GetProcedureName()] = procedure;
}

void AbstractProtocolHandler::HandleRequest(const std::string &request,
                                            std::string &retValue) {
  Json::Reader reader;
  Json::Value req;
  Json::Value resp;
  Json::FastWriter w;

  if (reader.parse(request, req, false)) {
    this->HandleJsonRequest(req, resp);
  } else {
    this->WrapError(Json::nullValue, ExceptionCode::ERROR_INVALID_JSON,
                    "JSON_PARSE_ERROR: The JSON-Object is not JSON-Valid",
                    resp);
  }

  if (resp != Json::nullValue)
    retValue = w.write(resp);
}

void AbstractProtocolHandler::ProcessRequest(const Json::Value &request,
                                             Json::Value &response) {
  Procedure &method =
      this->procedures[request[KEY_REQUEST_METHODNAME].asString()];
  Json::Value result;

  if (method.GetProcedureType() == RPC_METHOD) {
    handler.HandleMethodCall(method, request[KEY_REQUEST_PARAMETERS], result);
    this->WrapResult(request, response, result);
  } else {
    handler.HandleNotificationCall(method, request[KEY_REQUEST_PARAMETERS]);
    response = Json::nullValue;
  }
}

int AbstractProtocolHandler::ValidateRequest(const Json::Value &request) {
  int error = 0;
  Procedure proc;
  if (!this->ValidateRequestFields(request)) {
    error = ExceptionCode::ERROR_SERVER_INVALID_REQUEST;
  } else {
    map<string, Procedure>::iterator it =
        this->procedures.find(request[KEY_REQUEST_METHODNAME].asString());
    if (it != this->procedures.end()) {
      proc = it->second;
      if (this->GetRequestType(request) == RPC_METHOD &&
          proc.GetProcedureType() == RPC_NOTIFICATION) {
        error = ExceptionCode::ERROR_SERVER_PROCEDURE_IS_NOTIFICATION;
      } else if (this->GetRequestType(request) == RPC_NOTIFICATION &&
                 proc.GetProcedureType() == RPC_METHOD) {
        error = ExceptionCode::ERROR_SERVER_PROCEDURE_IS_METHOD;
      } else if (!proc.ValdiateParameters(request[KEY_REQUEST_PARAMETERS])) {
        error = ExceptionCode::ERROR_RPC_INVALID_PARAMS;
      }
    } else {
      error = ExceptionCode::ERROR_RPC_METHOD_NOT_FOUND;
    }
  }
  return error;
}