import http.server
import socketserver
import json
import os
import urllib.parse

PORT = 8002
DIRECTORY = os.path.dirname(os.path.abspath(__file__))

class AgentHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        super().end_headers()

    def do_GET(self):
        parsed_path = urllib.parse.urlparse(self.path)
        path = parsed_path.path
        
        if path == '/api/tree':
            files_list = []
            for root, dirs, files in os.walk(DIRECTORY):
                if '.git' in dirs:
                    dirs.remove('.git')
                if 'build' in dirs:
                    dirs.remove('build')
                for f in files:
                    if f.endswith(('.c', '.h', '.S', '.py', '.txt', '.md', '.html', '.css', '.js', '.json', '.ld', 'Makefile')):
                        full_path = os.path.join(root, f)
                        rel_path = os.path.relpath(full_path, DIRECTORY)
                        files_list.append(rel_path)
            
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(files_list).encode('utf-8'))
            
        elif path == '/api/file':
            query = urllib.parse.parse_qs(parsed_path.query)
            if 'path' in query:
                target_path = query['path'][0]
                full_target_path = os.path.abspath(os.path.join(DIRECTORY, target_path))
                
                if not full_target_path.startswith(DIRECTORY):
                    self.send_response(403)
                    self.end_headers()
                    self.wfile.write(b'{"error": "Access denied"}')
                    return
                
                if os.path.exists(full_target_path) and os.path.isfile(full_target_path):
                    with open(full_target_path, 'r', encoding='utf-8', errors='replace') as f:
                        content = f.read()
                    self.send_response(200)
                    self.send_header('Content-type', 'application/json')
                    self.end_headers()
                    self.wfile.write(json.dumps({"content": content}).encode('utf-8'))
                else:
                    self.send_response(404)
                    self.end_headers()
                    self.wfile.write(b'{"error": "File not found"}')
            else:
                self.send_response(400)
                self.end_headers()
                self.wfile.write(b'{"error": "Missing path parameter"}')
        else:
            super().do_GET()

with socketserver.TCPServer(("", PORT), AgentHandler) as httpd:
    print(f"Agent Server running at http://localhost:{PORT}")
    print(f"Serving directory: {DIRECTORY}")
    # Allow address reuse in case of rapid restarts
    httpd.allow_reuse_address = True
    httpd.serve_forever()
