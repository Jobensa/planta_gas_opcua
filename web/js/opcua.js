// OPC UA Management Module
class OPCUAManager {
    constructor() {
        this.opcuaTable = [];
        this.serverStructure = null;
        this.refreshInterval = null;
        this.refreshRate = 10000; // 10 seconds
        this.isActive = false;
    }

    init() {
        this.setupEventListeners();
    }

    setupEventListeners() {
        // Auto-refresh when tab becomes active
        document.addEventListener('visibilitychange', () => {
            if (document.hidden && this.isActive) {
                this.stop();
            } else if (!document.hidden && this.isActive) {
                this.start();
            }
        });
    }

    start() {
        this.isActive = true;
        this.loadOpcuaTable();
        this.loadServerStructure();
        
        this.refreshInterval = setInterval(() => {
            this.loadOpcuaTable();
        }, this.refreshRate);
    }

    stop() {
        this.isActive = false;
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
            this.refreshInterval = null;
        }
    }

    async loadOpcuaTable() {
        try {
            const response = await scadaAPI.getOpcuaTable();
            this.opcuaTable = response.table || [];
            this.renderOpcuaTable();
            
        } catch (error) {
            console.error('Error loading OPC UA table:', error);
            this.showError('Error cargando tabla OPC UA: ' + scadaAPI.formatError(error));
        }
    }

    async loadServerStructure() {
        try {
            // In a real implementation, this would fetch the server structure
            // For now, we'll create a mock structure based on the system
            const tags = await scadaAPI.getAllTags();
            this.serverStructure = this.createMockStructure(tags.tags || []);
            this.renderServerStructure();
            
        } catch (error) {
            console.error('Error loading server structure:', error);
            this.showError('Error cargando estructura del servidor: ' + scadaAPI.formatError(error));
        }
    }

    createMockStructure(tags) {
        const structure = {
            name: 'Objects',
            type: 'folder',
            children: [
                {
                    name: 'PlantaGas',
                    type: 'folder',
                    children: []
                }
            ]
        };

        // Group tags by category (based on tag prefix)
        const categories = {};
        tags.forEach(tag => {
            const prefix = tag.name.split('_')[0];
            if (!categories[prefix]) {
                categories[prefix] = [];
            }
            categories[prefix].push(tag);
        });

        // Create folder structure
        Object.keys(categories).forEach(category => {
            const categoryFolder = {
                name: category,
                type: 'folder',
                children: categories[category].map(tag => ({
                    name: tag.name,
                    type: 'variable',
                    dataType: tag.type,
                    value: tag.value,
                    quality: tag.quality
                }))
            };
            structure.children[0].children.push(categoryFolder);
        });

        return structure;
    }

    renderOpcuaTable() {
        const tbody = document.getElementById('opcua-table-body');
        if (!tbody) return;

        if (this.opcuaTable.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="3" class="text-center text-muted py-4">
                        <i class="fas fa-table fa-2x mb-2 d-block"></i>
                        Tabla OPC UA vacía
                    </td>
                </tr>
            `;
            return;
        }

        tbody.innerHTML = this.opcuaTable.map((entry, index) => this.createTableRow(entry, index)).join('');
    }

    createTableRow(entry, index) {
        const value = entry.value !== null && entry.value !== undefined ? 
                     this.formatTableValue(entry.value) : '-';
        
        const tagAssignment = entry.assigned_tag || '-';
        const rowClass = entry.assigned_tag ? '' : 'table-warning';
        
        return `
            <tr class="${rowClass}" data-index="${index}">
                <td>
                    <strong>${index}</strong>
                    <small class="text-muted d-block">0x${index.toString(16).padStart(4, '0').toUpperCase()}</small>
                </td>
                <td>
                    <input type="number" 
                           class="form-control form-control-sm" 
                           value="${value}" 
                           onchange="opcuaManager.updateTableValue(${index}, this.value)"
                           ${!entry.assigned_tag ? 'disabled' : ''}>
                    <small class="text-muted">Tipo: ${entry.data_type || 'Float'}</small>
                </td>
                <td>
                    <div class="d-flex justify-content-between align-items-center">
                        <span class="${entry.assigned_tag ? 'text-success fw-bold' : 'text-muted'}">
                            ${tagAssignment}
                        </span>
                        <div class="btn-group btn-group-sm">
                            <button type="button" 
                                    class="btn btn-outline-primary btn-sm"
                                    onclick="opcuaManager.showAssignDialog(${index})"
                                    title="Asignar tag">
                                <i class="fas fa-link"></i>
                            </button>
                            ${entry.assigned_tag ? `
                                <button type="button" 
                                        class="btn btn-outline-danger btn-sm"
                                        onclick="opcuaManager.unassignTag(${index})"
                                        title="Desasignar">
                                    <i class="fas fa-unlink"></i>
                                </button>
                            ` : ''}
                        </div>
                    </div>
                </td>
            </tr>
        `;
    }

    renderServerStructure() {
        const container = document.getElementById('opcua-structure');
        if (!container || !this.serverStructure) return;

        container.innerHTML = this.createStructureTree(this.serverStructure);
    }

    createStructureTree(node, level = 0) {
        const indent = '  '.repeat(level);
        const icon = node.type === 'folder' ? 'fas fa-folder' : 
                    node.type === 'variable' ? 'fas fa-tag' : 'fas fa-circle';
        
        const nodeClass = node.type === 'folder' ? 'opcua-folder' : 'opcua-node';
        
        let html = `
            <div class="opcua-tree" style="padding-left: ${level * 20}px">
                <div class="opcua-node ${nodeClass}">
                    <i class="${icon} me-2"></i>
                    <strong>${node.name}</strong>
                    ${node.type === 'variable' ? `
                        <small class="text-muted ms-2">
                            (${node.dataType}) = ${this.formatValue(node.value, node.dataType)}
                            <span class="badge badge-sm ms-1 ${this.getQualityClass(node.quality)}">${node.quality}</span>
                        </small>
                    ` : ''}
                </div>
            </div>
        `;

        if (node.children && node.children.length > 0) {
            html += node.children.map(child => this.createStructureTree(child, level + 1)).join('');
        }

        return html;
    }

    formatTableValue(value) {
        if (value === null || value === undefined) return '0.0';
        return parseFloat(value).toFixed(2);
    }

    formatValue(value, type) {
        if (value === null || value === undefined) return '-';
        
        switch (type) {
            case 'float':
                return parseFloat(value).toFixed(2);
            case 'bool':
                return value ? 'True' : 'False';
            case 'int':
                return parseInt(value).toString();
            default:
                return value.toString();
        }
    }

    getQualityClass(quality) {
        switch (quality) {
            case 'Good': return 'bg-success';
            case 'Bad': return 'bg-danger';
            case 'Uncertain': return 'bg-warning';
            default: return 'bg-secondary';
        }
    }

    async updateTableValue(index, value) {
        try {
            const numericValue = parseFloat(value);
            if (isNaN(numericValue)) {
                this.showError('Valor inválido');
                return;
            }

            await scadaAPI.setOpcuaValue(index, numericValue);
            this.showSuccess(`Valor actualizado en índice ${index}`);
            
            // Refresh the table to show the updated value
            setTimeout(() => this.loadOpcuaTable(), 1000);
            
        } catch (error) {
            this.showError('Error actualizando valor: ' + scadaAPI.formatError(error));
        }
    }

    async showAssignDialog(index) {
        try {
            // Get available tags
            const response = await scadaAPI.getAllTags();
            const tags = response.tags || [];
            
            const dialog = `
                <div class="modal fade" id="assignTagModal" tabindex="-1">
                    <div class="modal-dialog">
                        <div class="modal-content">
                            <div class="modal-header">
                                <h5 class="modal-title">Asignar Tag al Índice ${index}</h5>
                                <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                            </div>
                            <div class="modal-body">
                                <div class="mb-3">
                                    <label for="assignTagSelect" class="form-label">Seleccionar Tag:</label>
                                    <select class="form-select" id="assignTagSelect">
                                        <option value="">-- Seleccionar Tag --</option>
                                        ${tags.map(tag => `
                                            <option value="${tag.name}">${tag.name} (${tag.type})</option>
                                        `).join('')}
                                    </select>
                                </div>
                                <div class="mb-3">
                                    <small class="text-muted">
                                        El tag seleccionado será asociado al índice ${index} de la tabla OPC UA.
                                        Los valores del tag se sincronizarán automáticamente.
                                    </small>
                                </div>
                            </div>
                            <div class="modal-footer">
                                <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Cancelar</button>
                                <button type="button" class="btn btn-primary" onclick="opcuaManager.assignTag(${index})">Asignar</button>
                            </div>
                        </div>
                    </div>
                </div>
            `;

            // Remove existing modal if any
            const existingModal = document.getElementById('assignTagModal');
            if (existingModal) {
                existingModal.remove();
            }

            document.body.insertAdjacentHTML('beforeend', dialog);
            
            const modal = new bootstrap.Modal(document.getElementById('assignTagModal'));
            modal.show();

            // Clean up after hide
            modal._element.addEventListener('hidden.bs.modal', () => {
                modal._element.remove();
            });

        } catch (error) {
            this.showError('Error cargando tags: ' + scadaAPI.formatError(error));
        }
    }

    async assignTag(index) {
        const select = document.getElementById('assignTagSelect');
        const tagName = select.value;
        
        if (!tagName) {
            this.showError('Por favor seleccione un tag');
            return;
        }

        try {
            await scadaAPI.assignOpcuaIndex(tagName, index);
            this.showSuccess(`Tag "${tagName}" asignado al índice ${index}`);
            
            const modal = bootstrap.Modal.getInstance(document.getElementById('assignTagModal'));
            modal.hide();
            
            // Refresh the table
            setTimeout(() => this.loadOpcuaTable(), 1000);
            
        } catch (error) {
            this.showError('Error asignando tag: ' + scadaAPI.formatError(error));
        }
    }

    async unassignTag(index) {
        if (!confirm(`¿Está seguro de que desea desasignar el tag del índice ${index}?`)) {
            return;
        }

        try {
            // In a real implementation, there would be an unassign endpoint
            // For now, we'll simulate by assigning an empty tag name
            await scadaAPI.assignOpcuaIndex('', index);
            this.showSuccess(`Tag desasignado del índice ${index}`);
            
            setTimeout(() => this.loadOpcuaTable(), 1000);
            
        } catch (error) {
            this.showError('Error desasignando tag: ' + scadaAPI.formatError(error));
        }
    }

    showSuccess(message) {
        this.showToast(message, 'success');
    }

    showError(message) {
        this.showToast(message, 'danger');
    }

    showToast(message, type = 'info') {
        const alertClass = type === 'success' ? 'alert-success' : 
                          type === 'danger' ? 'alert-danger' : 'alert-info';
        
        const toast = document.createElement('div');
        toast.className = `alert ${alertClass} alert-dismissible position-fixed top-0 end-0 m-3`;
        toast.style.zIndex = '9999';
        toast.innerHTML = `
            ${message}
            <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
        `;
        
        document.body.appendChild(toast);
        
        setTimeout(() => {
            if (toast.parentNode) {
                toast.remove();
            }
        }, 5000);
    }
}

// Global OPC UA manager instance
window.opcuaManager = new OPCUAManager();