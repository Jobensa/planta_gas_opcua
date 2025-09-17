// API Communication Module
class ScadaAPI {
    constructor(baseUrl = 'http://localhost:8080') {
        this.baseUrl = baseUrl;
        this.isOnline = false;
        this.lastCheck = null;
    }

    async request(endpoint, options = {}) {
        const url = `${this.baseUrl}${endpoint}`;
        console.log(`🌐 API Request: ${options.method || 'GET'} ${url}`);
        
        try {
            const response = await fetch(url, {
                headers: {
                    'Content-Type': 'application/json',
                    ...options.headers
                },
                ...options
            });

            console.log(`📡 API Response: ${response.status} ${response.statusText}`);

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            const data = await response.json();
            console.log(`✅ API Success:`, data);
            this.isOnline = true;
            this.lastCheck = new Date();
            
            return data;
        } catch (error) {
            console.error(`❌ API Error (${endpoint}):`, error);
            
            // Only mark as offline for connection errors, not for 404s or other API errors
            if (error.message.includes('Failed to fetch') || error.message.includes('NetworkError')) {
                this.isOnline = false;
                console.log('🔌 Marking API as offline due to connection error');
            } else {
                // Keep online status for HTTP errors (404, 500, etc.) - these are server responses
                console.log('🌐 Keeping API online status - received server response');
            }
            
            this.lastCheck = new Date();
            throw error;
        }
    }

    // System endpoints
    async getHealth() {
        return this.request('/health');
    }

    async getStatus() {
        return this.request('/status');
    }

    async getStatistics() {
        return this.request('/statistics');
    }

    // Tag management endpoints
    async getAllTags() {
        console.log('🔍 API: Requesting all tags...');
        const result = await this.request('/tags');
        console.log('📦 API: Received tags response:', result);
        return result;
    }

    async getTag(tagName) {
        return this.request(`/tag/${encodeURIComponent(tagName)}`);
    }

    async createTag(tagData, tagType = null) {
        // Prepare data for API
        const apiData = {
            name: tagData.name,
            opcua_name: tagData.opcua_name || tagData.name,
            value_table: tagData.value_table,
            alarm_table: tagData.alarm_table || '',
            description: tagData.description || '',
            units: tagData.units || '',
            category: tagData.category || '',
            opcua_table_index: tagData.opcua_table_index || 0,
            variables: tagData.variables || [],
            data_type: 'FLOAT' // Default for industrial tags
        };

        // Add type-specific data
        if (tagType === 'instrument' && tagData.alarm_settings) {
            apiData.alarm_settings = tagData.alarm_settings;
        } else if (tagType === 'controller' && tagData.pid_settings) {
            apiData.pid_settings = tagData.pid_settings;
        }

        return this.request('/tags', {
            method: 'POST',
            body: JSON.stringify(apiData)
        });
    }

    async updateTag(tagName, tagData) {
        return this.request(`/tag/${encodeURIComponent(tagName)}`, {
            method: 'PUT',
            body: JSON.stringify(tagData)
        });
    }

    async deleteTag(tagName) {
        return this.request(`/tag/${encodeURIComponent(tagName)}`, {
            method: 'DELETE'
        });
    }

    // Template endpoints
    async getTemplates() {
        return this.request('/templates');
    }

    async getTemplate(templateName) {
        return this.request(`/template/${encodeURIComponent(templateName)}`);
    }

    // OPC UA endpoints
    async getOpcuaTable() {
        return this.request('/opcua-table');
    }

    async setOpcuaValue(index, value) {
        return this.request(`/opcua-table/${index}`, {
            method: 'PUT',
            body: JSON.stringify({ value: value })
        });
    }

    async assignOpcuaIndex(tagName, index) {
        return this.request('/opcua-assign', {
            method: 'POST',
            body: JSON.stringify({
                tag_name: tagName,
                index: index
            })
        });
    }

    // Backup endpoints
    async createBackup() {
        return this.request('/backup', {
            method: 'POST'
        });
    }

    async getBackups() {
        return this.request('/backups');
    }

    async restoreBackup(filename) {
        return this.request(`/backup/${encodeURIComponent(filename)}/restore`, {
            method: 'POST'
        });
    }

    // Configuration endpoints
    async validateConfig() {
        return this.request('/validate-config');
    }

    async getConfig() {
        return this.request('/config');
    }

    async updateConfig(config) {
        return this.request('/config', {
            method: 'PUT',
            body: JSON.stringify(config)
        });
    }

    // Utility methods
    getConnectionStatus() {
        return {
            online: this.isOnline,
            lastCheck: this.lastCheck
        };
    }

    formatError(error) {
        if (error.message.includes('Failed to fetch')) {
            return 'Error de conexión: No se puede conectar al servidor';
        }
        if (error.message.includes('HTTP 404')) {
            return 'Recurso no encontrado';
        }
        if (error.message.includes('HTTP 500')) {
            return 'Error interno del servidor';
        }
        return `Error: ${error.message}`;
    }
}

// Create global API instance
window.scadaAPI = new ScadaAPI();

// Global API instance
window.scadaAPI = new ScadaAPI();