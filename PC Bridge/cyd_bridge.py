import json
import re
import requests
from flask import Flask, request, jsonify, Response, stream_with_context

app = Flask(__name__)

# System state cache
state = {
    "active_model": "System Idle",
    "agent_status": "Ready",
    "tokens_used": 0
}

LM_STUDIO_URL = "http://localhost:1234"

@app.route('/update', methods=['POST'])
def manual_update():
    global state
    data = request.json
    if 'agent_status' in data: state['agent_status'] = data['agent_status']
    if 'active_model' in data: state['active_model'] = data['active_model']
    if 'tokens_used' in data: state['tokens_used'] += int(data['tokens_used'])
    return jsonify({"status": "success", "state": state}), 200

@app.route('/metrics', methods=['GET'])
def get_metrics():
    return jsonify(state), 200

@app.route('/v1/models', methods=['GET'])
def discover_models_proxy():
    try:
        upstream_response = requests.get(f"{LM_STUDIO_URL}/v1/models", timeout=3)
        return (upstream_response.content, upstream_response.status_code, upstream_response.headers.items())
    except requests.exceptions.ConnectionError:
        return jsonify({"object": "list", "data": []}), 200

@app.route('/v1/<path:path>', methods=['GET', 'POST', 'PUT', 'DELETE'])
def auto_proxy_middleware(path):
    global state
    upstream_url = f"{LM_STUDIO_URL}/v1/{path}"
    headers = {k: v for k, v in request.headers.items() if k.lower() != 'host'}
    
    if request.method == 'POST' and 'chat/completions' in path:
        req_payload = request.json or {}
        
        if "model" in req_payload:
            state["active_model"] = req_payload["model"].split("/")[-1]
            state["agent_status"] = "Processing..."

        if req_payload.get("stream", False):
            # Ensure LM Studio tracks usage without breaking stream structures
            if "stream_options" not in req_payload:
                req_payload["stream_options"] = {"include_usage": True}
                
            upstream_response = requests.post(upstream_url, json=req_payload, headers=headers, stream=True)
            
            def stream_processor():
                global state
                # Stream RAW chunks directly to preserve SSE formatting for the IDE
                for chunk in upstream_response.iter_content(chunk_size=1024):
                    if chunk:
                        decoded_chunk = chunk.decode('utf-8', errors='ignore')
                        # Quietly look for the usage data block at the end of the stream
                        if "total_tokens" in decoded_chunk:
                            token_match = re.search(r'"total_tokens"\s*:\s*(\d+)', decoded_chunk)
                            if token_match:
                                extracted_tokens = int(token_match.group(1))
                                state["tokens_used"] += extracted_tokens
                                state["agent_status"] = "Task Complete"
                        yield chunk
                        
            return Response(stream_with_context(stream_processor()), content_type=upstream_response.headers.get('Content-Type'))
        
        else:
            upstream_response = requests.post(upstream_url, json=req_payload, headers=headers)
            res_json = upstream_response.json()
            if "usage" in res_json:
                state["tokens_used"] += res_json["usage"].get("total_tokens", 0)
                state["agent_status"] = "Task Complete"
            return jsonify(res_json), upstream_response.status_code

    else:
        upstream_response = requests.request(
            method=request.method,
            url=upstream_url,
            headers=headers,
            data=request.get_data(),
            cookies=request.cookies,
            allow_redirects=False
        )
        return (upstream_response.content, upstream_response.status_code, upstream_response.headers.items())

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False)