// Tags Management Module
class TagsManager {
    constructor() {
        this.tags = [];
        this.filteredTags = [];
        this.currentSort = { field: 'name', direction: 'asc' };
        this.editingTag = null;
    }

    init() {
        this.setupEventListeners();
    }

    setupEventListeners() {
        // Search functionality
        const searchInput = document.getElementById('tag-search');
        if (searchInput) {
            searchInput.addEventListener('input', (e) => {
                this.filterTags();
            });
        }

        // Filter dropdowns
        const typeFilter = document.getElementById('tag-type-filter');
        const statusFilter = document.getElementById('tag-status-filter');
        
        if (typeFilter) {
            typeFilter.addEventListener('change', () => this.filterTags());
        }
        
        if (statusFilter) {
            statusFilter.addEventListener('change', () => this.filterTags());
        }

        // Refresh button
        const refreshBtn = document.getElementById('refresh-tags');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.loadTags());
        }

        // Modal form handling
        const saveBtn = document.getElementById('saveTag');
        if (saveBtn) {
            saveBtn.addEventListener('click', () => this.saveTag());
        }

        // Form validation
        const tagForm = document.getElementById('tagForm');
        if (tagForm) {
            tagForm.addEventListener('input', () => this.validateForm());
        }
    }

    async loadTags() {
        try {
            this.showLoading(true);
            
            const response = await scadaAPI.getAllTags();
            this.tags = response.tags || [];
            this.filteredTags = [...this.tags];
            
            this.renderTagsTable();
            this.showSuccess(`Cargados ${this.tags.length} tags`);
            
        } catch (error) {
            console.error('Error loading tags:', error);
            this.showError('Error cargando tags: ' + scadaAPI.formatError(error));
            this.tags = [];
            this.filteredTags = [];
            this.renderTagsTable();
        } finally {
            this.showLoading(false);
        }
    }

    filterTags() {
        const searchTerm = document.getElementById('tag-search')?.value.toLowerCase() || '';
        const typeFilter = document.getElementById('tag-type-filter')?.value || '';
        const statusFilter = document.getElementById('tag-status-filter')?.value || '';

        this.filteredTags = this.tags.filter(tag => {
            const matchesSearch = !searchTerm || 
                tag.name.toLowerCase().includes(searchTerm) ||
                tag.description?.toLowerCase().includes(searchTerm) || '';
            
            const matchesType = !typeFilter || tag.type === typeFilter;
            
            const matchesStatus = !statusFilter || 
                (statusFilter === 'active' && tag.quality === 'Good') ||
                (statusFilter === 'inactive' && tag.quality !== 'Good');

            return matchesSearch && matchesType && matchesStatus;
        });

        this.renderTagsTable();
    }

    renderTagsTable() {
        const tbody = document.getElementById('tags-table-body');
        if (!tbody) return;

        if (this.filteredTags.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="7" class="text-center text-muted py-4">
                        <i class="fas fa-inbox fa-2x mb-2 d-block"></i>
                        No hay tags disponibles
                    </td>
                </tr>
            `;
            return;
        }

        tbody.innerHTML = this.filteredTags.map(tag => this.createTagRow(tag)).join('');
    }

    createTagRow(tag) {
        const qualityBadge = this.getQualityBadge(tag.quality);
        const typeBadge = this.getTypeBadge(tag.type);
        const lastUpdate = tag.last_update ? new Date(tag.last_update).toLocaleString('es-ES') : '-';
        
        return `
            <tr data-tag-name="${tag.name}">
                <td>
                    <strong>${tag.name}</strong>
                    ${tag.unit ? `<small class="text-muted d-block">${tag.unit}</small>` : ''}
                </td>
                <td>${typeBadge}</td>
                <td>
                    <span class="fw-bold">${this.formatValue(tag.value, tag.type)}</span>
                    ${tag.limits ? `<small class="text-muted d-block">${tag.limits.min} - ${tag.limits.max}</small>` : ''}
                </td>
                <td>${qualityBadge}</td>
                <td>
                    ${tag.description || '-'}
                    ${tag.pac_address ? `<small class="text-muted d-block">PAC: ${tag.pac_address}</small>` : ''}
                </td>
                <td>
                    <small>${lastUpdate}</small>
                </td>
                <td>
                    <div class="btn-group btn-group-sm" role="group">
                        <button type="button" class="btn btn-outline-primary" onclick="tagsManager.editTag('${tag.name}')" title="Editar">
                            <i class="fas fa-edit"></i>
                        </button>
                        <button type="button" class="btn btn-outline-info" onclick="tagsManager.viewTagDetails('${tag.name}')" title="Ver detalles">
                            <i class="fas fa-eye"></i>
                        </button>
                        <button type="button" class="btn btn-outline-danger" onclick="tagsManager.deleteTag('${tag.name}')" title="Eliminar">
                            <i class="fas fa-trash"></i>
                        </button>
                    </div>
                </td>
            </tr>
        `;
    }

    getQualityBadge(quality) {
        const badges = {
            'Good': '<span class="badge bg-success">Buena</span>',
            'Bad': '<span class="badge bg-danger">Mala</span>',
            'Uncertain': '<span class="badge bg-warning">Incierta</span>',
            'Unknown': '<span class="badge bg-secondary">Desconocida</span>'
        };
        return badges[quality] || badges['Unknown'];
    }

    getTypeBadge(type) {
        const badges = {
            'float': '<span class="badge bg-info">Float</span>',
            'bool': '<span class="badge bg-success">Boolean</span>',
            'int': '<span class="badge bg-primary">Integer</span>',
            'string': '<span class="badge bg-secondary">String</span>'
        };
        return badges[type] || `<span class="badge bg-secondary">${type}</span>`;
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

    async editTag(tagName) {
        try {
            const tag = await scadaAPI.getTag(tagName);
            this.editingTag = tag;
            this.populateModal(tag);
            
            document.getElementById('tagModalTitle').textContent = 'Editar Tag';
            const modal = new bootstrap.Modal(document.getElementById('tagModal'));
            modal.show();
            
        } catch (error) {
            this.showError('Error cargando tag: ' + scadaAPI.formatError(error));
        }
    }

    async viewTagDetails(tagName) {
        try {
            const tag = await scadaAPI.getTag(tagName);
            this.showTagDetails(tag);
            
        } catch (error) {
            this.showError('Error cargando detalles del tag: ' + scadaAPI.formatError(error));
        }
    }

    showTagDetails(tag) {
        const details = `
            <div class="modal fade" id="tagDetailsModal" tabindex="-1">
                <div class="modal-dialog modal-lg">
                    <div class="modal-content">
                        <div class="modal-header">
                            <h5 class="modal-title">Detalles del Tag: ${tag.name}</h5>
                            <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                        </div>
                        <div class="modal-body">
                            <div class="row">
                                <div class="col-md-6">
                                    <h6>Información General</h6>
                                    <table class="table table-sm">
                                        <tr><td><strong>Nombre:</strong></td><td>${tag.name}</td></tr>
                                        <tr><td><strong>Tipo:</strong></td><td>${tag.type}</td></tr>
                                        <tr><td><strong>Valor:</strong></td><td>${this.formatValue(tag.value, tag.type)}</td></tr>
                                        <tr><td><strong>Calidad:</strong></td><td>${this.getQualityBadge(tag.quality)}</td></tr>
                                        <tr><td><strong>Unidad:</strong></td><td>${tag.unit || '-'}</td></tr>
                                        <tr><td><strong>Descripción:</strong></td><td>${tag.description || '-'}</td></tr>
                                    </table>
                                </div>
                                <div class="col-md-6">
                                    <h6>Configuración PAC</h6>
                                    <table class="table table-sm">
                                        <tr><td><strong>Dirección PAC:</strong></td><td>${tag.pac_address || '-'}</td></tr>
                                        <tr><td><strong>Valor Mín:</strong></td><td>${tag.limits?.min || '-'}</td></tr>
                                        <tr><td><strong>Valor Máx:</strong></td><td>${tag.limits?.max || '-'}</td></tr>
                                        <tr><td><strong>Última Actualización:</strong></td><td>${tag.last_update ? new Date(tag.last_update).toLocaleString('es-ES') : '-'}</td></tr>
                                    </table>
                                    
                                    ${tag.history && tag.history.length > 0 ? `
                                        <h6>Historial Reciente</h6>
                                        <div style="max-height: 150px; overflow-y: auto;">
                                            <table class="table table-sm">
                                                <thead>
                                                    <tr><th>Tiempo</th><th>Valor</th></tr>
                                                </thead>
                                                <tbody>
                                                    ${tag.history.slice(-10).map(entry => `
                                                        <tr>
                                                            <td><small>${new Date(entry.timestamp).toLocaleString('es-ES')}</small></td>
                                                            <td>${this.formatValue(entry.value, tag.type)}</td>
                                                        </tr>
                                                    `).join('')}
                                                </tbody>
                                            </table>
                                        </div>
                                    ` : ''}
                                </div>
                            </div>
                        </div>
                        <div class="modal-footer">
                            <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Cerrar</button>
                        </div>
                    </div>
                </div>
            </div>
        `;

        // Remove existing modal if any
        const existingModal = document.getElementById('tagDetailsModal');
        if (existingModal) {
            existingModal.remove();
        }

        // Add new modal to body
        document.body.insertAdjacentHTML('beforeend', details);
        
        // Show modal
        const modal = new bootstrap.Modal(document.getElementById('tagDetailsModal'));
        modal.show();

        // Clean up after hide
        modal._element.addEventListener('hidden.bs.modal', () => {
            modal._element.remove();
        });
    }

    populateModal(tag) {
        document.getElementById('tagName').value = tag.name || '';
        document.getElementById('tagType').value = tag.type || '';
        document.getElementById('tagAddress').value = tag.pac_address || '';
        document.getElementById('tagUnit').value = tag.unit || '';
        document.getElementById('tagDescription').value = tag.description || '';
        document.getElementById('tagMinValue').value = tag.limits?.min || '';
        document.getElementById('tagMaxValue').value = tag.limits?.max || '';
    }

    clearModal() {
        document.getElementById('tagForm').reset();
        this.editingTag = null;
        document.getElementById('tagModalTitle').textContent = 'Nuevo Tag';
    }

    validateForm() {
        const name = document.getElementById('tagName').value.trim();
        const type = document.getElementById('tagType').value;
        const saveBtn = document.getElementById('saveTag');
        
        const isValid = name && type;
        saveBtn.disabled = !isValid;
        
        return isValid;
    }

    async saveTag() {
        if (!this.validateForm()) return;

        const tagData = {
            name: document.getElementById('tagName').value.trim(),
            type: document.getElementById('tagType').value,
            pac_address: document.getElementById('tagAddress').value.trim() || null,
            unit: document.getElementById('tagUnit').value.trim() || null,
            description: document.getElementById('tagDescription').value.trim() || null,
            limits: {}
        };

        const minValue = document.getElementById('tagMinValue').value;
        const maxValue = document.getElementById('tagMaxValue').value;
        
        if (minValue) tagData.limits.min = parseFloat(minValue);
        if (maxValue) tagData.limits.max = parseFloat(maxValue);
        
        if (Object.keys(tagData.limits).length === 0) {
            delete tagData.limits;
        }

        try {
            this.showLoading(true);
            
            if (this.editingTag) {
                await scadaAPI.updateTag(this.editingTag.name, tagData);
                this.showSuccess('Tag actualizado correctamente');
            } else {
                await scadaAPI.createTag(tagData);
                this.showSuccess('Tag creado correctamente');
            }

            const modal = bootstrap.Modal.getInstance(document.getElementById('tagModal'));
            modal.hide();
            
            this.clearModal();
            await this.loadTags();
            
        } catch (error) {
            this.showError('Error guardando tag: ' + scadaAPI.formatError(error));
        } finally {
            this.showLoading(false);
        }
    }

    async deleteTag(tagName) {
        if (!confirm(`¿Está seguro de que desea eliminar el tag "${tagName}"?`)) {
            return;
        }

        try {
            this.showLoading(true);
            
            await scadaAPI.deleteTag(tagName);
            this.showSuccess('Tag eliminado correctamente');
            await this.loadTags();
            
        } catch (error) {
            this.showError('Error eliminando tag: ' + scadaAPI.formatError(error));
        } finally {
            this.showLoading(false);
        }
    }

    showLoading(show) {
        const spinner = document.getElementById('loading-spinner');
        if (spinner) {
            spinner.classList.toggle('d-none', !show);
        }
    }

    showSuccess(message) {
        this.showToast(message, 'success');
    }

    showError(message) {
        this.showToast(message, 'danger');
    }

    showToast(message, type = 'info') {
        // Simple toast implementation - could use Bootstrap Toast for better UX
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
        
        // Auto-remove after 5 seconds
        setTimeout(() => {
            if (toast.parentNode) {
                toast.remove();
            }
        }, 5000);
    }
}

// Global tags manager instance
window.tagsManager = new TagsManager();