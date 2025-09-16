// Tags Management Module
class TagsManager {
    constructor() {
        this.tags = [];
        this.filteredTags = [];
        this.currentSort = { field: 'name', direction: 'asc' };
        this.editingTag = null;
        this.currentTagType = null; // 'instrument' or 'controller'
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

        // Tag Type Selection Cards
        const typeCards = document.querySelectorAll('.tag-type-card');
        typeCards.forEach(card => {
            card.addEventListener('click', () => {
                const type = card.getAttribute('data-type');
                this.openTagForm(type);
            });
        });

        // Modal form handling
        const saveInstrumentBtn = document.getElementById('saveInstrument');
        if (saveInstrumentBtn) {
            saveInstrumentBtn.addEventListener('click', () => this.saveInstrument());
        }

        const saveControllerBtn = document.getElementById('saveController');
        if (saveControllerBtn) {
            saveControllerBtn.addEventListener('click', () => this.saveController());
        }

        // Auto-fill table names based on tag name
        this.setupAutoFillListeners();

        // Form validation
        this.setupFormValidation();
    }

    openTagForm(type) {
        this.currentTagType = type;
        
        // Close type selection modal
        const typeModal = bootstrap.Modal.getInstance(document.getElementById('tagTypeModal'));
        if (typeModal) {
            typeModal.hide();
        }

        // Open appropriate form modal
        setTimeout(() => {
            if (type === 'instrument') {
                const modal = new bootstrap.Modal(document.getElementById('instrumentModal'));
                modal.show();
                this.resetInstrumentForm();
            } else if (type === 'controller') {
                const modal = new bootstrap.Modal(document.getElementById('controllerModal'));
                modal.show();
                this.resetControllerForm();
            }
        }, 300);
    }

    detectTagType(tagName) {
        const name = tagName.toUpperCase();
        
        // Instrument prefixes
        if (name.startsWith('ET_') || name.startsWith('FIT_') || 
            name.startsWith('TIT_') || name.startsWith('PIT_') || 
            name.startsWith('LIT_') || name.startsWith('PDIT_')) {
            return 'instrument';
        }
        
        // Controller prefixes
        if (name.startsWith('FRC_') || name.startsWith('TRC_') || name.startsWith('PRC_')) {
            return 'controller';
        }
        
        return 'unknown';
    }

    setupAutoFillListeners() {
        // Instrument form auto-fill
        const instName = document.getElementById('inst_name');
        if (instName) {
            instName.addEventListener('input', (e) => {
                const name = e.target.value.toUpperCase();
                const valueTable = document.getElementById('inst_value_table');
                const alarmTable = document.getElementById('inst_alarm_table');
                
                if (name && valueTable && alarmTable) {
                    valueTable.value = `TBL_${name}`;
                    alarmTable.value = `TBL_TA_${name}`;
                }
                
                this.autoSelectCategory('instrument', name);
            });
        }

        // Controller form auto-fill
        const ctrlName = document.getElementById('ctrl_name');
        if (ctrlName) {
            ctrlName.addEventListener('input', (e) => {
                const name = e.target.value.toUpperCase();
                const valueTable = document.getElementById('ctrl_value_table');
                const alarmTable = document.getElementById('ctrl_alarm_table');
                
                if (name && valueTable && alarmTable) {
                    valueTable.value = `TBL_${name}`;
                    alarmTable.value = `TBL_TA_${name}`;
                }
                
                this.autoSelectCategory('controller', name);
            });
        }
    }

    autoSelectCategory(type, tagName) {
        if (type === 'instrument') {
            const categorySelect = document.getElementById('inst_category');
            const unitsSelect = document.getElementById('inst_units');
            
            if (tagName.startsWith('ET_')) {
                categorySelect.value = 'FLOW_TRANSMITTER';
                unitsSelect.value = 'm3/h';
            } else if (tagName.startsWith('FIT_')) {
                categorySelect.value = 'FLOW_INDICATOR';
                unitsSelect.value = 'm3/h';
            } else if (tagName.startsWith('TIT_')) {
                categorySelect.value = 'TEMPERATURE_INDICATOR';
                unitsSelect.value = '°C';
            } else if (tagName.startsWith('PIT_')) {
                categorySelect.value = 'PRESSURE_INDICATOR';
                unitsSelect.value = 'bar';
            } else if (tagName.startsWith('LIT_')) {
                categorySelect.value = 'LEVEL_INDICATOR';
                unitsSelect.value = 'm';
            } else if (tagName.startsWith('PDIT_')) {
                categorySelect.value = 'PRESSURE_DIFFERENTIAL';
                unitsSelect.value = 'bar';
            }
        } else if (type === 'controller') {
            const categorySelect = document.getElementById('ctrl_category');
            const unitsSelect = document.getElementById('ctrl_units');
            
            if (tagName.startsWith('FRC_')) {
                categorySelect.value = 'FLOW_CONTROLLER';
                unitsSelect.value = 'm3/h';
            } else if (tagName.startsWith('TRC_')) {
                categorySelect.value = 'TEMPERATURE_CONTROLLER';
                unitsSelect.value = '°C';
            } else if (tagName.startsWith('PRC_')) {
                categorySelect.value = 'PRESSURE_CONTROLLER';
                unitsSelect.value = 'bar';
            }
        }
    }

    setupFormValidation() {
        // Basic validation setup for both forms
        const instrumentForm = document.getElementById('instrumentForm');
        const controllerForm = document.getElementById('controllerForm');
        
        if (instrumentForm) {
            instrumentForm.addEventListener('input', () => this.validateInstrumentForm());
        }
        
        if (controllerForm) {
            controllerForm.addEventListener('input', () => this.validateControllerForm());
        }
    }

    resetInstrumentForm() {
        const form = document.getElementById('instrumentForm');
        if (form) {
            form.reset();
            this.editingTag = null;
            document.getElementById('instrumentModalTitle').textContent = 'Nuevo Instrumento de Medición';
        }
    }

    resetControllerForm() {
        const form = document.getElementById('controllerForm');
        if (form) {
            form.reset();
            this.editingTag = null;
            document.getElementById('controllerModalTitle').textContent = 'Nuevo Controlador PID';
            // Set default PID values
            document.getElementById('ctrl_kp').value = '1.0';
            document.getElementById('ctrl_ki').value = '0.1';
            document.getElementById('ctrl_kd').value = '0.01';
            document.getElementById('ctrl_output_low').value = '0';
            document.getElementById('ctrl_output_high').value = '100';
            document.getElementById('ctrl_pid_enable').checked = true;
        }
    }

    validateInstrumentForm() {
        const name = document.getElementById('inst_name').value;
        const category = document.getElementById('inst_category').value;
        const units = document.getElementById('inst_units').value;
        const valueTable = document.getElementById('inst_value_table').value;
        
        const isValid = name && category && units && valueTable;
        const saveBtn = document.getElementById('saveInstrument');
        if (saveBtn) {
            saveBtn.disabled = !isValid;
        }
        
        return isValid;
    }

    validateControllerForm() {
        const name = document.getElementById('ctrl_name').value;
        const category = document.getElementById('ctrl_category').value;
        const units = document.getElementById('ctrl_units').value;
        const valueTable = document.getElementById('ctrl_value_table').value;
        
        const isValid = name && category && units && valueTable;
        const saveBtn = document.getElementById('saveController');
        if (saveBtn) {
            saveBtn.disabled = !isValid;
        }
        
        return isValid;
    }

    async saveInstrument() {
        if (!this.validateInstrumentForm()) {
            this.showError('Por favor completa todos los campos requeridos');
            return;
        }

        try {
            const instrumentData = {
                name: document.getElementById('inst_name').value,
                opcua_name: document.getElementById('inst_name').value,
                value_table: document.getElementById('inst_value_table').value,
                alarm_table: document.getElementById('inst_alarm_table').value || '',
                description: document.getElementById('inst_description').value,
                units: document.getElementById('inst_units').value,
                category: document.getElementById('inst_category').value,
                opcua_table_index: parseInt(document.getElementById('inst_opcua_index').value) || 0,
                variables: [
                    "PV", "SV", "SetHH", "SetH", "SetL", "SetLL", 
                    "Input", "percent", "min", "max", "SIM_Value"
                ]
            };

            // Add alarm settings if provided
            const alarmSettings = {};
            const setHH = document.getElementById('inst_set_hh').value;
            const setH = document.getElementById('inst_set_h').value;
            const setL = document.getElementById('inst_set_l').value;
            const setLL = document.getElementById('inst_set_ll').value;
            const min = document.getElementById('inst_min').value;
            const max = document.getElementById('inst_max').value;

            if (setHH) alarmSettings.SetHH = parseFloat(setHH);
            if (setH) alarmSettings.SetH = parseFloat(setH);
            if (setL) alarmSettings.SetL = parseFloat(setL);
            if (setLL) alarmSettings.SetLL = parseFloat(setLL);
            if (min) alarmSettings.min = parseFloat(min);
            if (max) alarmSettings.max = parseFloat(max);

            if (Object.keys(alarmSettings).length > 0) {
                instrumentData.alarm_settings = alarmSettings;
            }

            await this.saveTagData(instrumentData, 'instrument');
            
            // Close modal
            const modal = bootstrap.Modal.getInstance(document.getElementById('instrumentModal'));
            if (modal) modal.hide();
            
        } catch (error) {
            console.error('Error saving instrument:', error);
            this.showError('Error al guardar el instrumento: ' + error.message);
        }
    }

    async saveController() {
        if (!this.validateControllerForm()) {
            this.showError('Por favor completa todos los campos requeridos');
            return;
        }

        try {
            const controllerData = {
                name: document.getElementById('ctrl_name').value,
                opcua_name: document.getElementById('ctrl_name').value,
                value_table: document.getElementById('ctrl_value_table').value,
                alarm_table: document.getElementById('ctrl_alarm_table').value || '',
                description: document.getElementById('ctrl_description').value,
                units: document.getElementById('ctrl_units').value,
                category: document.getElementById('ctrl_category').value,
                opcua_table_index: parseInt(document.getElementById('ctrl_opcua_index').value) || 0,
                variables: [
                    "PV", "SP", "CV", "KP", "KI", "KD", 
                    "auto_manual", "OUTPUT_HIGH", "OUTPUT_LOW", "PID_ENABLE"
                ]
            };

            // Add PID settings
            const pidSettings = {
                KP: parseFloat(document.getElementById('ctrl_kp').value) || 1.0,
                KI: parseFloat(document.getElementById('ctrl_ki').value) || 0.1,
                KD: parseFloat(document.getElementById('ctrl_kd').value) || 0.01,
                OUTPUT_LOW: parseInt(document.getElementById('ctrl_output_low').value) || 0,
                OUTPUT_HIGH: parseInt(document.getElementById('ctrl_output_high').value) || 100,
                PID_ENABLE: document.getElementById('ctrl_pid_enable').checked,
                auto_manual: document.getElementById('ctrl_auto_manual').checked
            };

            controllerData.pid_settings = pidSettings;

            await this.saveTagData(controllerData, 'controller');
            
            // Close modal
            const modal = bootstrap.Modal.getInstance(document.getElementById('controllerModal'));
            if (modal) modal.hide();
            
        } catch (error) {
            console.error('Error saving controller:', error);
            this.showError('Error al guardar el controlador: ' + error.message);
        }
    }

    async saveTagData(tagData, tagType) {
        const response = await scadaAPI.createTag(tagData, tagType);
        
        if (response.success) {
            this.showSuccess(`${tagType === 'instrument' ? 'Instrumento' : 'Controlador'} guardado correctamente`);
            await this.loadTags(); // Reload the tags list
        } else {
            throw new Error(response.message || 'Error desconocido');
        }
    }

    async loadTags() {
        console.log('🔄 Loading tags...');
        try {
            this.showLoading(true);
            
            console.log('📡 Fetching tags from API...');
            const response = await scadaAPI.getAllTags();
            console.log('📦 API Response:', response);
            
            // API returns 'data' not 'tags'
            this.tags = response.data || [];
            this.filteredTags = [...this.tags];
            
            console.log(`✅ Loaded ${this.tags.length} tags`);
            
            this.renderTagsTable();
            this.showSuccess(`Cargados ${this.tags.length} tags`);
            
        } catch (error) {
            console.error('❌ Error loading tags:', error);
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
        console.log('🎨 Rendering tags table...');
        const tbody = document.getElementById('tags-table-body');
        if (!tbody) {
            console.warn('⚠️  Table body element "tags-table-body" not found!');
            return;
        }

        console.log(`📊 Rendering ${this.filteredTags.length} tags`);

        if (this.filteredTags.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="6" class="text-center text-muted py-4">
                        <i class="fas fa-inbox fa-2x mb-2 d-block"></i>
                        No hay tags disponibles
                    </td>
                </tr>
            `;
            console.log('📝 Rendered empty state');
            return;
        }

        tbody.innerHTML = this.filteredTags.map(tag => this.createTagRow(tag)).join('');
        console.log('✅ Tags table rendered successfully');
    }

    createTagRow(tag) {
        // Determine tag type based on name prefix
        const tagType = this.detectTagType(tag.name);
        const typeBadge = this.getTypeBadge(tagType);
        
        const lastUpdate = tag.last_update ? new Date(tag.last_update * 1000).toLocaleString('es-ES') : '-';
        
        // Status based on available data
        const status = tag.is_critical ? 'Crítico' : 'Normal';
        const statusClass = tag.is_critical ? 'bg-danger' : 'bg-success';
        
        return `
            <tr data-tag-name="${tag.name}">
                <td>
                    <strong>${tag.name}</strong>
                    ${tag.units ? `<small class="text-muted d-block">${tag.units}</small>` : ''}
                </td>
                <td>${typeBadge}</td>
                <td>
                    <span class="fw-bold">-</span>
                    <small class="text-muted d-block">Variables: ${tag.variable_count || 0}</small>
                </td>
                <td>
                    <span class="badge ${statusClass}">${status}</span>
                </td>
                <td>
                    ${tag.description || '-'}
                    ${tag.category ? `<small class="text-muted d-block">Categoría: ${tag.category}</small>` : ''}
                </td>
                <td>
                    <small>${lastUpdate}</small>
                </td>
                <td>
                    <div class="btn-group btn-group-sm" role="group">
                        <button type="button" class="btn btn-outline-primary" onclick="tagsManager.editTag('${tag.name}')" title="Editar">
                            <i class="fas fa-edit"></i>
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