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
    port = 8000
    
    # Change to web directory
    web_dir = "/media/jose/Datos/Data/Proyectos/PetroSantander/SCADA/pac_to_opcua/planta_gas/web"
    if os.path.exists(web_dir):
        os.chdir(web_dir)
    else:
        print(f"Error: Web directory not found: {web_dir}")
        sys.exit(1)
    
    # Start server
    with socketserver.TCPServer(("127.0.0.1", port), CORSRequestHandler) as httpd:
        print(f"🌐 SCADA Web Server iniciado en http://127.0.0.1:{port}")
        print(f"📁 Sirviendo archivos desde: {os.getcwd()}")
        print("🔗 Accede a: http://127.0.0.1:8000")
        print("Press Ctrl+C to stop")
        
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n🛑 Servidor detenido")

if __name__ == "__main__":
    main()