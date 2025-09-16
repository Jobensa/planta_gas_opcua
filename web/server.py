#!/usr/bin/env python3
"""
Simple HTTP server for SCADA Web Interface
Serves index.html by default and handles CORS
"""

import http.server
import socketserver
import os
import sys
from urllib.parse import urlparse

class CORSRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type, Authorization')
        super().end_headers()
    
    def do_GET(self):
        # If requesting root, serve index.html
        if self.path == '/':
            self.path = '/index.html'
        return super().do_GET()

def main():
    PORT = 8081
    
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    
    with socketserver.TCPServer(("", PORT), CORSRequestHandler) as httpd:
        print(f"🌐 SCADA Web Server iniciado en http://127.0.0.1:{PORT}")
        print(f"📁 Sirviendo archivos desde: {os.getcwd()}")
        print(f"🔗 Accede a: http://127.0.0.1:{PORT}")
        print("Press Ctrl+C to stop")
        
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n🛑 Servidor detenido")

if __name__ == "__main__":
    main()